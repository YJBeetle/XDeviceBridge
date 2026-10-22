// SPDX-License-Identifier: LGPL-3.0-only
#include "Backend.h"
#include "ddi/Download.h"
#include "hid/Hid.h"
#include "media/FramePump.h"
#include "remote/App.h"
#include "remote/Discovery.h"
#include "remote/DisplayInfo.h"
#include "remote/Pairing.h"
#include "remote/WifiPairing.h"
#include "transport/IoBudget.h"
#include "wifi/PairRecord.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <thread>
namespace xdb::server {
namespace {
int bounded(const Json &p, const char *key, int fallback, int min, int max) {
    if (!p.contains(key))
        return fallback;
    if (!p[key].is_number_integer())
        throw std::invalid_argument(std::string(key) + " must be an integer");
    auto v = p[key].get<int64_t>();
    if (v < min || v > max)
        throw std::invalid_argument(std::string(key) + " is out of range");
    return int(v);
}
std::vector<wifi::PairRecord> records(const std::string &dir, const Cancel &cancel) {
    std::vector<std::string> warnings;
    bool stopped = false;
    return remote::detail::load_discovery_records(dir, cancel, warnings, stopped);
}
Json discovery_json(const remote::DiscoveryResult &value) {
    Json devices = Json::array();
    for (const auto &device : value.devices) {
        Json candidates = Json::array();
        for (const auto &c : device.candidates)
            candidates.push_back(
                {{"transport", c.transport == remote::DiscoveryTransport::usbmux ? "usb" : "wifi"},
                 {"address", c.address},
                 {"port", c.port},
                 {"interface", c.interface_name},
                 {"paired", c.pairing == remote::DiscoveryPairing::ready},
                 {"connection_type", c.connection_type}});
        devices.push_back({{"udid", device.udid}, {"name", device.name}, {"candidates", candidates}});
    }
    return {{"devices", devices}, {"warnings", value.warnings}, {"cancelled", value.cancelled}};
}
bool unit(double n) { return std::isfinite(n) && n >= 0 && n <= 1; }
} // namespace
std::optional<remote::Device> open_device(const std::string &udid, const std::string &wifi_address,
                                          std::string &error, uint16_t port, const Cancel &cancel,
                                          const std::string &directory) {
    transport::IoBudget budget({std::chrono::seconds(30), cancel});
    if (wifi_address.empty())
        return remote::Device::establish_bounded(udid, budget, error, false, true);
    auto paired = records(directory, cancel);
    auto record = std::find_if(paired.begin(), paired.end(), [&](const auto &r) {
        return r.udid == udid && r.complete() && r.has_peer_identity();
    });
    if (record == paired.end()) {
        error = "No usable pairing for the requested UDID";
        return std::nullopt;
    }
    if (wifi_address != "auto")
        return remote::Device::establish_wifi_bounded(wifi_address, *record, budget, error, false, port);
    remote::DiscoveryOptions options;
    options.include_usb = false;
    options.pairing_directory = directory;
    options.should_cancel = cancel;
    auto discovery = remote::discover_devices(options);
    for (const auto &device : discovery.devices) {
        if (device.udid != udid)
            continue;
        for (const auto &c : device.candidates) {
            if (c.transport != remote::DiscoveryTransport::remote_pairing ||
                c.pairing != remote::DiscoveryPairing::ready || c.address.empty())
                continue;
            auto connected =
                remote::Device::establish_wifi_bounded(c.address, *record, budget, error, false, c.port);
            if (connected)
                return connected;
            if (cancel())
                return std::nullopt;
        }
    }
    if (error.empty())
        error = "No paired Wi-Fi device discovered";
    return std::nullopt;
}
struct Backend::Session {
    // Reverse destruction keeps all services/worker references inside Device's lifetime.
    remote::Device device;
    std::unique_ptr<remote::DisplayWatcher> display;
    std::unique_ptr<hid::Service> hid;
    std::unique_ptr<hid::Buttons> buttons;
    std::unique_ptr<media::FramePump> video;
    std::set<uint16_t> keys;
    std::set<std::pair<uint16_t, uint16_t>> held_buttons;
    bool touching = false;
    double x = 0, y = 0;
    explicit Session(remote::Device value) : device(std::move(value)) {}
    ~Session() {
        std::string ignored;
        if (hid) {
            if (touching)
                hid->touch(hid::kSurfaceMainTouchscreen, x, y, false, ignored);
            hid->send_report(hid::kSurfaceKeyboard, hid::keyboard_report({}), ignored);
        }
        if (buttons)
            for (auto [page, code] : held_buttons)
                buttons->release(page, code, ignored);
    }
};
Backend::Backend(Registry &r, std::string dir) : registry_(r), directory_(std::move(dir)) {}
Backend::~Backend() { close(); }
void Backend::close() {
    session_.reset();
    if (!lease_.empty())
        registry_.release(lease_);
    lease_.clear();
}
Reply Backend::handle(const std::string &method, const Json &p, const Event &event, const Cancel &cancel) {
    auto fail = [](const std::string &error) { return Reply::failure("operation_failed", error); };
    std::string error;
    if (cancel())
        return Reply::failure("cancelled", "Request cancelled");
    if (method == "devices") {
        remote::DiscoveryOptions o;
        o.pairing_directory = directory_;
        o.should_cancel = cancel;
        o.timeout = std::chrono::milliseconds(bounded(p, "timeout_ms", 3000, 0, 60000));
        auto mode = p.value("transport", std::string("auto"));
        if (mode != "usb" && mode != "wifi" && mode != "auto")
            return Reply::failure("invalid_params", "Unknown transport");
        o.include_usb = mode != "wifi";
        o.include_wifi = mode != "usb";
        auto value = remote::discover_devices(o);
        Reply r;
        r.result = discovery_json(value);
        return r;
    }
    if (method == "pair") {
        auto udid = p.value("udid", std::string());
        auto lease = udid.empty() ? std::string("@pairing") : udid;
        if (!registry_.acquire(lease))
            return Reply::failure("device_busy", "Device is already in use");
        struct Release {
            Registry &r;
            std::string id;
            ~Release() { r.release(id); }
        } release{registry_, lease};
        auto progress = [&](std::string_view text) { event("progress", {{"message", text}}); };
        remote::PairingResult value;
        auto mode = p.value("transport", std::string("wifi"));
        if (mode != "wifi" && mode != "usb")
            return Reply::failure("invalid_params", "Choose usb or wifi pairing");
        if (mode == "wifi") {
            remote::WifiPairingOptions o;
            o.udid = udid;
            o.pairing_directory = directory_;
            o.should_cancel = cancel;
            o.progress = progress;
            o.timeout_ms = bounded(p, "timeout_ms", 120000, 1, 300000);
            o.display_pin = [&](std::string_view pin, std::string &) {
                event("pin", {{"pin", pin}});
                return !cancel();
            };
            value = remote::pair_wifi_remote(o);
        } else {
            remote::UsbPairingOptions o;
            o.udid = udid;
            o.pairing_directory = directory_;
            o.should_cancel = cancel;
            o.progress = progress;
            o.timeout_ms = bounded(p, "timeout_ms", 120000, 1, 300000);
            o.allow_repair = p.value("repair", false);
            value = remote::pair_usb_remote(o);
        }
        if (!value.ok)
            return fail(value.error);
        Reply r;
        r.result = {{"udid", value.udid}, {"reused", value.reused}};
        return r;
    }
    if (method == "pairing.import") {
        auto text = p.at("record").get<std::string>();
        if (text.size() > 16384)
            return Reply::failure("invalid_params", "Pairing record exceeds 16 KiB");
        auto record = wifi::parse_record(text, error);
        if (!record || !record->complete() || !record->has_peer_identity())
            return fail(error.empty() ? "Incomplete pairing record" : error);
        auto path = wifi::record_path(directory_, record->udid);
        if (!wifi::save_record(path, *record, error))
            return fail(error);
        Reply r;
        r.result = {{"udid", record->udid}};
        return r;
    }
    if (method == "ddi.download") {
        ddi::DownloadOptions o;
        o.cancelled = cancel;
        o.timeout = std::chrono::seconds(bounded(p, "timeout_seconds", 300, 1, 3600));
        auto cache = p.value("cache_directory", std::string());
        if (!cache.empty())
            o.cache_root = std::filesystem::u8path(cache);
        o.progress = [&](const std::string &file, uint64_t bytes, uint64_t total) {
            event("progress", {{"file", file}, {"bytes", bytes}, {"total", total}});
        };
        ddi::DownloadTarget target;
        target.system_version = p.at("os_version").get<std::string>();
        target.product_type = p.value("product_type", std::string());
        auto image = ddi::download_for_target(o, target, error);
        if (!image)
            return fail(error);
        Reply r;
        r.result = {{"build", image->build}, {"cache_hit", image->cache_hit}};
        return r;
    }
    if (method == "ddi.prepare") {
        PrepareOptions o;
        o.serial = p.at("udid").get<std::string>();
        o.wifi = p.value("wifi", std::string());
        if (o.serial.empty())
            return Reply::failure("invalid_params", "UDID is required");
        if (!registry_.acquire(o.serial))
            return Reply::failure("device_busy", "Close the device session before preparing DDI");
        struct Release {
            Registry &r;
            std::string id;
            ~Release() { r.release(id); }
        } release{registry_, o.serial};
        o.data_directory = directory_;
        o.ddi_directory = p.value("cache_directory", std::string());
        o.wifi_port = uint16_t(bounded(p, "port", 49152, 1, 65535));
        o.replace_ddi = p.value("replace", false);
        o.ddi_download_timeout = bounded(p, "download_timeout_seconds", 300, 1, 3600);
        o.ddi_install_timeout = bounded(p, "install_timeout_seconds", 300, 1, 3600);
        if (!prepare_ddi(o, error, event, cancel))
            return fail(error);
        Reply r;
        r.result = {{"prepared", true}};
        return r;
    }
    if (method == "connection.close") {
        close();
        return {};
    }
    if (method == "connection.open") {
        if (session_)
            return Reply::failure("invalid_state", "This client already owns a device session");
        auto udid = p.at("udid").get<std::string>();
        if (udid.empty())
            return Reply::failure("invalid_params", "UDID is required");
        if (!registry_.acquire(udid))
            return Reply::failure("device_busy", "Device is already in use");
        lease_ = udid;
        try {
            auto device = open_device(udid, p.value("wifi", std::string()), error,
                                      uint16_t(bounded(p, "port", 49152, 1, 65535)), cancel, directory_);
            if (!device) {
                close();
                return fail(error);
            }
            session_ = std::make_unique<Session>(std::move(*device));
        } catch (...) {
            close();
            throw;
        }
        Reply r;
        r.result = {{"udid", session_->device.udid()}};
        return r;
    }
    if (!session_)
        return Reply::failure("not_connected", "Open a device session first");
    auto &s = *session_;
    if (method == "device.info") {
        Reply r;
        r.result = {{"udid", s.device.udid()},
                    {"os_version", s.device.property("OSVersion")},
                    {"product_type", s.device.property("ProductType")}};
        return r;
    }
    if (method == "screenshot") {
        auto input = xpc::make_dict();
        xpc::dict_set(input, "displayUniqueID", xpc::make_null());
        xpc::dict_set(input, "requestedFormat", xpc::make_string("png"));
        xpc::Value answer;
        if (!s.device.feature("com.apple.coredevice.screencaptureservice",
                              "com.apple.coredevice.feature.capturescreenshot",
                              "com.apple.coredevice.action.capturescreenshot", input, answer, error))
            return fail(error);
        auto *image = answer.find("image");
        if (!image || image->data.empty())
            return fail("Screenshot response has no image data");
        Reply r;
        r.result = {{"format", "png"}};
        r.binary = image->data;
        return r;
    }
    if (method == "video.start") {
        if (!kHaveDecoder)
            return Reply::failure("unsupported", "No HEVC decoder in this build");
        if (!s.video) {
            if (!s.display)
                s.display = remote::DisplayWatcher::start(s.device, 1, error);
            media::FramePump::Options options;
            options.use_hardware = p.value("hardware_decode", false);
            s.video = media::FramePump::start(s.device, options, error);
        }
        if (!s.video)
            return fail(error);
        return {};
    }
    if (method == "video.stop") {
        s.video.reset();
        return {};
    }
    if (method == "video.frame") {
        if (!s.video)
            return Reply::failure("invalid_state", "Video is not running");
        Frame frame;
        auto timeout = bounded(p, "timeout_ms", 50, 0, 1000);
        const auto since = p.value("since", uint64_t(0));
        auto serial = s.video->newer(frame, since, timeout);
        if (!serial)
            return Reply::failure("no_frame", "No newer frame within the requested timeout");
        Reply r;
        r.result = {{"format", "bgra"},
                    {"serial", serial},
                    {"width", frame.width},
                    {"height", frame.height},
                    {"row_pitch", frame.row_pitch}};
        if (s.display) {
            auto geometry = s.display->latest();
            r.result["display"] = {{"width", geometry.width},
                                   {"height", geometry.height},
                                   {"orientation", geometry.orientation},
                                   {"serial", geometry.seq}};
        }
        r.binary = std::move(frame.pixels);
        return r;
    }
    if (method == "app.list") {
        std::vector<remote::App::Entry> apps;
        if (!remote::App::list(s.device, apps, error))
            return fail(error);
        Reply r;
        r.result = Json::array();
        for (const auto &app : apps)
            r.result.push_back({{"bundle_id", app.bundle_id}, {"name", app.name}});
        return r;
    }
    if (method == "app.launch" || method == "app.stop") {
        const auto bundle = p.at("bundle_id").get<std::string>();
        if (bundle.empty())
            return Reply::failure("invalid_params", "bundle_id is required");
        bool ok = method == "app.launch"
                      ? remote::App::launch(s.device, bundle, error, p.value("terminate_existing", false))
                      : remote::App::stop(s.device, bundle, error);
        return ok ? Reply{} : fail(error);
    }
    if (method.starts_with("input.")) {
        if (!s.hid)
            s.hid = hid::Service::open(s.device, error);
        if (!s.hid)
            return fail(error);
        if (s.video)
            s.video->wake();
        if (method == "input.touch" || method == "input.tap") {
            auto x = p.at("x").get<double>(), y = p.at("y").get<double>();
            if (!unit(x) || !unit(y))
                return Reply::failure("invalid_params", "Touch coordinates must be in [0,1]");
            s.x = x;
            s.y = y;
            if (method == "input.tap") {
                s.touching = true;
                const bool ok = s.hid->tap(x, y, bounded(p, "hold_ms", 30, 0, 2000), error);
                if (ok)
                    s.touching = false;
                return ok ? Reply{} : fail(error);
            }
            bool down = p.at("down").get<bool>();
            if (down)
                s.touching = true;
            const bool ok = s.hid->touch(hid::kSurfaceMainTouchscreen, x, y, down, error);
            if (ok)
                s.touching = down;
            return ok ? Reply{} : fail(error);
        }
        if (method == "input.text") {
            auto text = p.at("text").get<std::string>();
            if (text.size() > 4096)
                return Reply::failure("invalid_params", "Text exceeds 4096 bytes");
            for (unsigned char c : text)
                if ((c < 32 && c != '\t' && c != '\n') || c > 126)
                    return Reply::failure("unsupported", "Only US keyboard ASCII text is supported");
            return s.hid->type_text(text, 0, error) ? Reply{} : fail(error);
        }
        if (method == "input.keyboard") {
            auto usages = p.at("usages").get<std::vector<uint16_t>>();
            for (auto key : usages)
                if (key >= 240)
                    return Reply::failure("invalid_params", "Keyboard usage must be below 240");
            s.keys = {usages.begin(), usages.end()};
            return s.hid->send_report(hid::kSurfaceKeyboard, hid::keyboard_report(usages), error)
                       ? Reply{}
                       : fail(error);
        }
        if (method == "input.button") {
            auto page = uint16_t(bounded(p, "page", 12, 0, 65535)),
                 code = uint16_t(bounded(p, "usage", 0, 0, 65535));
            bool down = p.at("down").get<bool>();
            if (!s.buttons)
                s.buttons = hid::Buttons::open(s.device, error);
            if (!s.buttons)
                return fail(error);
            if (down)
                s.held_buttons.insert({page, code});
            const bool ok = down ? s.buttons->down(page, code, error) : s.buttons->release(page, code, error);
            if (ok && !down)
                s.held_buttons.erase({page, code});
            return ok ? Reply{} : fail(error);
        }
    }
    return Reply::failure("unknown_method", "Unknown method: " + method);
}
} // namespace xdb::server
