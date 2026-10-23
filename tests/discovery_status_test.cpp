// SPDX-License-Identifier: LGPL-3.0-only
#include "server/Discovery.h"
#include <stdexcept>

void check(bool value) {
    if (!value)
        throw std::runtime_error("Discovery status mismatch");
}

int main() {
    using namespace xdb;
    remote::DiscoveryResult found;
    remote::DiscoveryCandidate usb;
    remote::DiscoveryCandidate wifi;
    wifi.transport = remote::DiscoveryTransport::remote_pairing;
    wifi.pairing = remote::DiscoveryPairing::ready;
    found.devices = {{"usb-phone", "", {usb}}, {"paired-phone", "Phone", {wifi}},
                     {"active-phone", "", {usb}}, {"", "Unknown", {}}};
    auto devices = server::discovery_json(found, {"active-phone"})["devices"];
    check(devices[0]["state"] == "discovered" && devices[0]["connection"] == "unverified");
    check(devices[0]["pairing"] == "unknown" && !devices[0]["candidates"][0]["paired"].get<bool>());
    check(devices[1]["state"] == "paired" && devices[1]["connection"] == "unverified");
    check(devices[1]["candidates"][0]["pairing"] == "record_present");
    check(devices[2]["state"] == "connected" && devices[2]["connection"] == "connected");
    check(devices[3]["udid"] == "" && devices[3]["state"] == "discovered");
    check(std::string(server::pairing_state(remote::DiscoveryPairing::identifier_hint)) == "identifier_hint");
    check(std::string(server::pairing_state(remote::DiscoveryPairing::needs_pairing)) == "incomplete_record");
}
