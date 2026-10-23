// SPDX-License-Identifier: LGPL-3.0-only
#pragma once
#include "xdb.h"
#include <atomic>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace xdb::client {
using Json = nlohmann::json;
struct Response {
    Json value = Json::object();
    std::vector<uint8_t> data;
    std::string code;
};
// Header-only convenience adapter. Only the C ABI crosses the library boundary.
class Client {
  public:
    Client() = default;
    Client(const Client &) = delete;
    Client &operator=(const Client &) = delete;
    ~Client() { close(); }
    bool connect(std::string &error, uint16_t port = 0,
                 xdb_event_callback event = nullptr, void *userdata = nullptr) {
        close();
        handle_ = xdb_client_connect(port, event, userdata);
        if (!handle_)
            error = xdb_last_error();
        return handle_ != nullptr;
    }
    bool request(const char *method, const Json &params, Response &out, std::string &error) {
        out = {};
        xdb_response *raw = nullptr;
        auto text = params.dump();
        if (!xdb_client_request(handle_, method, text.c_str(), &raw)) {
            error = xdb_last_error();
            out.code = "transport_error";
            return false;
        }
        std::unique_ptr<xdb_response, decltype(&xdb_response_destroy)> response(raw, xdb_response_destroy);
        auto envelope = Json::parse(xdb_response_json(raw), nullptr, false);
        if (!envelope.is_object() || !envelope.value("ok", false)) {
            auto failure = envelope.is_object() ? envelope.value("error", Json::object()) : Json::object();
            error = failure.value("message", std::string("Invalid xdb response"));
            out.code = failure.value("code", std::string());
            return false;
        }
        out.value = envelope.value("result", Json::object());
        if (auto size = xdb_response_size(raw)) {
            auto data = xdb_response_data(raw);
            out.data.assign(data, data + size);
        }
        error.clear();
        return true;
    }
    bool request(const char *method, const Json &params, std::string &error) {
        Response out;
        return request(method, params, out, error);
    }
    void interrupt() { xdb_client_interrupt(handle_); }
    void close() { xdb_client_destroy(handle_.exchange(nullptr)); }
    bool connected() const { return handle_ != nullptr; }

  private:
    std::atomic<xdb_client *> handle_{nullptr};
};
} // namespace xdb::client
