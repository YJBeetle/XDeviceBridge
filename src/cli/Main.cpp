// SPDX-License-Identifier: LGPL-3.0-only
#include "ipc/Json.h"
#include "xdb/xdb.h"
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>
using Json = nlohmann::json;
namespace {
volatile std::sig_atomic_t stopping = 0;
void stop(int) { stopping = 1; }
const char *env(const char *name) {
    auto *value = std::getenv(name);
    return value ? value : "";
}
void event(const char *message, void *) {
    auto value = xdb::ipc::parse_json(message);
    auto data = value.value("data", Json::object());
    if (value.value("event", std::string()) == "pin")
        std::cerr << "Pairing code: " << data.value("pin", std::string()) << '\n';
    else if (data.contains("message"))
        std::cerr << data["message"].get<std::string>() << '\n';
    else
        std::cerr << data.dump() << '\n';
}
std::filesystem::path data_directory() {
#ifdef _WIN32
    if (*env("LOCALAPPDATA"))
        return std::filesystem::u8path(env("LOCALAPPDATA")) / "XDeviceBridge";
#elif defined(__APPLE__)
    if (*env("HOME"))
        return std::filesystem::path(env("HOME")) / "Library" / "Application Support" / "XDeviceBridge";
#else
    if (*env("XDG_DATA_HOME"))
        return std::filesystem::path(env("XDG_DATA_HOME")) / "XDeviceBridge";
    if (*env("HOME"))
        return std::filesystem::path(env("HOME")) / ".local" / "share" / "XDeviceBridge";
#endif
    throw std::invalid_argument("Cannot locate user data directory; supply --data-dir");
}
int number(const std::string &value, int minimum, int maximum) {
    size_t used = 0;
    auto n = std::stoll(value, &used);
    if (used != value.size() || n < minimum || n > maximum)
        throw std::invalid_argument("Numeric option out of range");
    return int(n);
}
void help() {
    std::cout << "XDeviceBridge 0.1.0\n"
                 "xdb start-server [--data-dir DIR] [--port PORT]\n"
                 "xdb kill-server [--port PORT]\n"
                 "xdb server [--data-dir DIR] [--port PORT]\n"
                 "xdb devices [-l] [--usb|--wifi] [--timeout MS]\n"
                 "xdb [-s UDID] pair [--wifi|--usb] [--timeout MS]\n"
                 "xdb -s UDID ddi prepare [--wifi [ADDRESS]] [--cache-dir DIR] [--replace]\n"
                 "xdb -s UDID screenshot FILE [--wifi [ADDRESS]]\n"
                 "xdb pairing import FILE\n"
                 "xdb rpc METHOD [JSON_PARAMS] [--output FILE]\n"
                 "xdb rpc-shell\n"
                 "Client options: --port PORT (or XDB_PORT).\n"
                 "Server listens on loopback only. Default port: 17423. --port 0 selects a free port.\n"
                 "Desktop clients automatically start a background server when needed.\n"
                 "Use server to run in the foreground for diagnostics.\n"
                 "rpc-shell is a JSON debugging console, not an iOS command shell.\n";
}
} // namespace
int main(int argc, char **argv) {
    try {
        if (argc < 2 || std::string(argv[1]) == "--help") {
            help();
            return 0;
        }
        uint16_t port = *env("XDB_PORT") ? uint16_t(number(env("XDB_PORT"), 1, 65535)) : 17423;
        std::string dir, output, serial, wifi_address, cache;
        bool wifi = false, usb = false, replace = false, json_output = false;
        int timeout = -1;
        std::vector<std::string> args;
        for (int i = 1; i < argc; ++i) {
            std::string a = argv[i];
            if (a == "--wifi") {
                wifi = true;
                wifi_address = "auto";
                if (i + 1 < argc) {
                    std::string next = argv[i + 1];
                    if (next == "auto" || next.find('.') != std::string::npos ||
                        next.find(':') != std::string::npos) {
                        wifi_address = next;
                        ++i;
                    }
                }
            } else if (a == "--usb")
                usb = true;
            else if (a == "--replace")
                replace = true;
            else if (a == "--json")
                json_output = true;
            else if (a == "-l") {
            } else if (a == "--port" || a == "--data-dir" || a == "--output" || a == "-s" ||
                       a == "--serial" || a == "--timeout" || a == "--cache-dir") {
                if (++i == argc)
                    throw std::invalid_argument("Missing value for " + a);
                if (a == "--port")
                    port = uint16_t(number(argv[i], 0, 65535));
                else if (a == "--data-dir")
                    dir = argv[i];
                else if (a == "--output")
                    output = argv[i];
                else if (a == "-s" || a == "--serial")
                    serial = argv[i];
                else if (a == "--timeout")
                    timeout = number(argv[i], 0, 300000);
                else
                    cache = argv[i];
            } else if (a.starts_with('-'))
                throw std::invalid_argument("Unknown option: " + a);
            else
                args.push_back(a);
        }
        if (wifi && usb)
            throw std::invalid_argument("Choose --usb or --wifi");
        if (args.empty())
            throw std::invalid_argument("Missing command");
        if (args[0] == "server") {
            if (args.size() != 1)
                throw std::invalid_argument("Unexpected server argument");
            if (dir.empty())
                dir = data_directory().string();
            auto *server = xdb_server_start(port, dir.c_str());
            if (!server) {
                std::cerr << xdb_server_last_error() << '\n';
                return 1;
            }
            std::unique_ptr<xdb_server, decltype(&xdb_server_stop)> owned(server, xdb_server_stop);
            std::signal(SIGINT, stop);
            std::signal(SIGTERM, stop);
            std::cout << Json{{"ready", true},
                              {"protocol", 1},
                              {"host", "127.0.0.1"},
                              {"port", xdb_server_port(server)}}
                             .dump()
                      << std::endl;
            while (!stopping && xdb_server_is_running(server))
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return 0;
        }
        if (args[0] == "start-server" || args[0] == "kill-server") {
            if (args.size() != 1 || !port)
                throw std::invalid_argument("Server lifecycle commands require a fixed port and no extra arguments");
            auto ok = args[0] == "start-server"
                          ? xdb_client_start_server(port, dir.empty() ? nullptr : dir.c_str())
                          : xdb_client_kill_server(port);
            if (!ok)
                std::cerr << xdb_last_error() << '\n';
            return ok ? 0 : 1;
        }
        if (args[0] != "devices" && args[0] != "pair" && args[0] != "ddi" && args[0] != "screenshot" &&
            args[0] != "pairing" && args[0] != "rpc" && args[0] != "rpc-shell")
            throw std::invalid_argument("Unknown command; run xdb --help");
        if (!dir.empty() && !xdb_client_start_server(port, dir.c_str())) {
            std::cerr << xdb_last_error() << '\n';
            return 1;
        }
        std::unique_ptr<xdb_client, decltype(&xdb_client_destroy)> client(
            xdb_client_connect(port, event, nullptr), xdb_client_destroy);
        if (!client) {
            std::cerr << xdb_last_error() << '\n';
            return 1;
        }
        auto request = [&](const std::string &method, const Json &params, bool display = true) {
            auto text = params.dump();
            xdb_response *raw = nullptr;
            if (!xdb_client_request(client.get(), method.c_str(), text.c_str(), &raw))
                throw std::runtime_error(xdb_last_error());
            std::unique_ptr<xdb_response, decltype(&xdb_response_destroy)> response(raw,
                                                                                    xdb_response_destroy);
            auto value = xdb::ipc::parse_json(xdb_response_json(raw));
            if (!value.value("ok", false)) {
                if (display && json_output)
                    std::cout << value.dump() << '\n';
                else
                    std::cerr << value["error"].value("message", std::string("Request failed")) << '\n';
                return value;
            }
            if (xdb_response_size(raw)) {
                if (output.empty())
                    throw std::invalid_argument("Binary response requires --output FILE");
                std::ofstream file(std::filesystem::u8path(output), std::ios::binary);
                file.write(reinterpret_cast<const char *>(xdb_response_data(raw)), xdb_response_size(raw));
                if (!file)
                    throw std::runtime_error("Failed to write binary output");
            }
            if (display) {
                if (json_output || args[0] == "rpc" || args[0] == "rpc-shell")
                    std::cout << value.dump() << std::endl;
                else if (method == "devices") {
                    std::cout << "List of iOS devices attached\n";
                    for (const auto &device : value["result"]["devices"]) {
                        std::cout << device.value("udid", std::string("(unpaired)")) << "\t"
                                  << device.value("name", std::string()) << '\n';
                        for (const auto &candidate : device["candidates"])
                            std::cout << "  " << candidate["transport"].get<std::string>() << " "
                                      << candidate["address"].get<std::string>() << ":" << candidate["port"]
                                      << (candidate["paired"].get<bool>() ? " paired" : "") << '\n';
                    }
                    for (const auto &warning : value["result"]["warnings"])
                        std::cerr << "warning: " << warning.get<std::string>() << '\n';
                } else if (method == "pair" || method == "pairing.import")
                    std::cout << "Paired: " << value["result"].value("udid", std::string()) << '\n';
                else
                    std::cout << "OK\n";
            }
            return value;
        };
        if (args[0] == "rpc-shell") {
            std::string line;
            while (std::getline(std::cin, line)) {
                if (line.empty())
                    continue;
                auto v = xdb::ipc::parse_json(line);
                request(v.at("method").get<std::string>(), v.value("params", Json::object()));
            }
            return 0;
        }
        if (args[0] == "rpc") {
            if (args.size() < 2 || args.size() > 3)
                throw std::invalid_argument("rpc requires METHOD [JSON_PARAMS]");
            return request(args[1], args.size() == 3 ? xdb::ipc::parse_json(args[2]) : Json::object())
                           .value("ok", false)
                       ? 0
                       : 1;
        }
        Json params = Json::object();
        if (!serial.empty())
            params["udid"] = serial;
        if (args[0] == "devices" || args[0] == "pair") {
            if (args.size() != 1)
                throw std::invalid_argument("Unexpected argument");
            params["transport"] = usb ? "usb" : (wifi || args[0] == "pair" ? "wifi" : "auto");
            if (timeout >= 0)
                params["timeout_ms"] = timeout;
            return request(args[0], params).value("ok", false) ? 0 : 1;
        }
        if (args[0] == "pairing") {
            if (args.size() != 3 || args[1] != "import")
                throw std::invalid_argument("pairing import requires FILE");
            std::ifstream file(std::filesystem::u8path(args[2]), std::ios::binary);
            std::string record;
            char data[16385];
            file.read(data, sizeof(data));
            record.assign(data, size_t(file.gcount()));
            if (!file.eof() || record.size() > 16384)
                throw std::invalid_argument("Pairing file is unreadable or exceeds 16 KiB");
            return request("pairing.import", {{"record", record}}).value("ok", false) ? 0 : 1;
        }
        if (serial.empty())
            throw std::invalid_argument("Select a device with -s UDID");
        if (wifi)
            params["wifi"] = wifi_address;
        if (args[0] == "ddi" && args.size() == 2 && args[1] == "prepare") {
            params["replace"] = replace;
            if (!cache.empty())
                params["cache_directory"] = cache;
            return request("ddi.prepare", params).value("ok", false) ? 0 : 1;
        }
        if (args[0] == "screenshot" && args.size() == 2) {
            output = args[1];
            if (!request("connection.open", params, false).value("ok", false))
                return 1;
            return request("screenshot", Json::object()).value("ok", false) ? 0 : 1;
        }
        throw std::invalid_argument("Unknown command; run xdb --help");
    } catch (const std::exception &e) {
        std::cerr << "xdb: " << e.what() << '\n';
        return 2;
    }
}
