// SPDX-License-Identifier: LGPL-3.0-only
#include "ipc/Json.h"
#include "ipc/Wire.h"
#include "xdb/xdb.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
using Json = nlohmann::json;
struct xdb_response {
    xdb::ipc::Message message;
};
struct xdb_client {
    xdb::transport::Socket socket;
    std::mutex mutex;
    uint64_t next_id = 1;
    xdb_event_callback event = nullptr;
    void *userdata = nullptr;
};
namespace {
thread_local std::string diagnostic;
}
extern "C" {
const char *xdb_last_error() { return diagnostic.c_str(); }
xdb_client *xdb_client_connect(uint16_t port, const char *token, xdb_event_callback event, void *userdata) {
    diagnostic.clear();
    try {
        if (!port || !token || !*token) {
            diagnostic = "port and server token are required";
            return nullptr;
        }
        auto client = std::make_unique<xdb_client>();
        client->socket = xdb::ipc::connect(port, diagnostic);
        if (!client->socket.valid())
            return nullptr;
        client->socket.set_read_timeout(5000, diagnostic);
        client->event = event;
        client->userdata = userdata;
        xdb_response *raw = nullptr;
        auto params = Json{{"protocol", 1}, {"token", token}}.dump();
        if (!xdb_client_request(client.get(), "hello", params.c_str(), &raw))
            return nullptr;
        std::unique_ptr<xdb_response> response(raw);
        auto value = xdb::ipc::parse_json(response->message.json);
        if (!value.value("ok", false)) {
            diagnostic = "xdb handshake rejected";
            return nullptr;
        }
        client->socket.set_read_timeout(0, diagnostic);
        return client.release();
    } catch (const std::exception &e) {
        diagnostic = e.what();
        return nullptr;
    }
}
int xdb_client_request(xdb_client *client, const char *method, const char *params, xdb_response **out) {
    diagnostic.clear();
    if (out)
        *out = nullptr;
    if (!client || !method || !out) {
        diagnostic = "invalid request arguments";
        return 0;
    }
    try {
        std::lock_guard lock(client->mutex);
        auto p = xdb::ipc::parse_json(params ? params : "{}");
        if (!p.is_object()) {
            diagnostic = "params must be an object";
            return 0;
        }
        auto id = client->next_id++;
        xdb::ipc::Message request{Json{{"id", id}, {"method", method}, {"params", p}}.dump(), {}};
        if (!xdb::ipc::write(client->socket, request, diagnostic))
            return 0;
        for (;;) {
            auto response = std::make_unique<xdb_response>();
            if (!xdb::ipc::read(client->socket, response->message, diagnostic))
                return 0;
            auto value = xdb::ipc::parse_json(response->message.json);
            if (!value.is_object() || value.value("id", uint64_t(0)) != id) {
                diagnostic = "unexpected response id";
                return 0;
            }
            if (value.contains("event")) {
                if (client->event)
                    client->event(response->message.json.c_str(), client->userdata);
                continue;
            }
            if (!value.contains("ok") || !value["ok"].is_boolean()) {
                diagnostic = "invalid response";
                return 0;
            }
            *out = response.release();
            return 1;
        }
    } catch (const std::exception &e) {
        diagnostic = e.what();
        if (client)
            client->socket.interrupt();
        return 0;
    }
}
void xdb_client_interrupt(xdb_client *client) {
    if (client)
        client->socket.interrupt();
}
void xdb_client_destroy(xdb_client *client) { delete client; }
const char *xdb_response_json(const xdb_response *response) {
    return response ? response->message.json.c_str() : "";
}
const uint8_t *xdb_response_data(const xdb_response *response) {
    return response && !response->message.binary.empty() ? response->message.binary.data() : nullptr;
}
size_t xdb_response_size(const xdb_response *response) {
    return response ? response->message.binary.size() : 0;
}
void xdb_response_destroy(xdb_response *response) { delete response; }
}
