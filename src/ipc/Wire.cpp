// SPDX-License-Identifier: LGPL-3.0-only
#include "Wire.h"
#include <array>
#include <cstring>
#include <limits>
#ifndef _WIN32
#include <arpa/inet.h>
#endif
namespace xdb::ipc {
namespace {
uint32_t decode(const uint8_t *p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
void encode(uint8_t *p, uint32_t value) {
    for (int i = 3; i >= 0; --i) {
        p[i] = uint8_t(value);
        value >>= 8;
    }
}
} // namespace
bool read(transport::Socket &socket, Message &out, std::string &error) {
    out = {};
    std::array<uint8_t, 16> header{};
    if (!socket.read_exact(header.data(), header.size(), error))
        return false;
    if (std::memcmp(header.data(), "XDB1", 4) != 0 || decode(header.data() + 4) != protocol_version) {
        error = "unsupported xdb wire version";
        return false;
    }
    const auto json_size = decode(header.data() + 8), binary_size = decode(header.data() + 12);
    if (!json_size || json_size > max_json || binary_size > max_binary) {
        error = "xdb message exceeds size limit";
        return false;
    }
    out.json.resize(json_size);
    out.binary.resize(binary_size);
    return socket.read_exact(out.json.data(), out.json.size(), error) &&
           (out.binary.empty() || socket.read_exact(out.binary.data(), out.binary.size(), error));
}
bool write(transport::Socket &socket, const Message &message, std::string &error) {
    if (message.json.empty() || message.json.size() > max_json || message.binary.size() > max_binary) {
        error = "xdb message exceeds size limit";
        return false;
    }
    std::array<uint8_t, 16> header{};
    std::memcpy(header.data(), "XDB1", 4);
    encode(header.data() + 4, protocol_version);
    encode(header.data() + 8, uint32_t(message.json.size()));
    encode(header.data() + 12, uint32_t(message.binary.size()));
    return socket.write_all(header.data(), header.size(), error) &&
           socket.write_all(message.json.data(), message.json.size(), error) &&
           (message.binary.empty() || socket.write_all(message.binary.data(), message.binary.size(), error));
}
transport::Socket connect(uint16_t port, std::string &error) {
    if (!transport::initialize_sockets(error))
        return {};
    transport::Socket socket(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (!socket.valid()) {
        error = transport::socket_error_message();
        return {};
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (::connect(socket.fd(), reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
        error = transport::socket_error_message();
        return {};
    }
    return socket;
}
} // namespace xdb::ipc
