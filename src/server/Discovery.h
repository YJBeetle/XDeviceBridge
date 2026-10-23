// SPDX-License-Identifier: LGPL-3.0-only
#pragma once
#include "remote/Discovery.h"
#include <nlohmann/json.hpp>
#include <set>

namespace xdb::server {
inline const char *pairing_state(remote::DiscoveryPairing value) {
    using remote::DiscoveryPairing;
    switch (value) {
    case DiscoveryPairing::ready: return "record_present";
    case DiscoveryPairing::needs_pairing: return "incomplete_record";
    case DiscoveryPairing::identifier_hint: return "identifier_hint";
    case DiscoveryPairing::ambiguous: return "ambiguous";
    case DiscoveryPairing::invalid_advertisement: return "invalid_advertisement";
    case DiscoveryPairing::crypto_error: return "crypto_error";
    case DiscoveryPairing::unmatched: return "unknown";
    }
    return "unknown";
}
inline nlohmann::json discovery_json(const remote::DiscoveryResult &value,
                                     const std::set<std::string> &connected) {
    using Json = nlohmann::json;
    Json devices = Json::array();
    for (const auto &device : value.devices) {
        Json candidates = Json::array();
        bool paired = false;
        for (const auto &c : device.candidates) {
            const bool record_present = c.pairing == remote::DiscoveryPairing::ready;
            paired |= record_present;
            candidates.push_back(
                {{"transport", c.transport == remote::DiscoveryTransport::usbmux ? "usb" : "wifi"},
                 {"address", c.address}, {"port", c.port}, {"interface", c.interface_name},
                 {"paired", record_present}, {"pairing", pairing_state(c.pairing)},
                 {"connection_type", c.connection_type}});
        }
        const bool active = connected.contains(device.udid);
        devices.push_back({{"udid", device.udid}, {"name", device.name}, {"candidates", candidates},
                           {"state", active ? "connected" : paired ? "paired" : "discovered"},
                           {"pairing", paired ? "record_present" : "unknown"},
                           {"connection", active ? "connected" : "unverified"}});
    }
    return {{"devices", devices}, {"warnings", value.warnings}, {"cancelled", value.cancelled}};
}
} // namespace xdb::server
