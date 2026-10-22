// SPDX-License-Identifier: LGPL-3.0-only
#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string_view>
namespace xdb::ipc {
inline nlohmann::json parse_json(std::string_view text) {
    if (text.size() > 1024 * 1024)
        throw std::invalid_argument("JSON exceeds 1 MiB");
    return nlohmann::json::parse(text, [](int depth, nlohmann::json::parse_event_t, auto &) {
        if (depth > 64)
            throw std::invalid_argument("JSON exceeds maximum nesting depth");
        return true;
    });
}
} // namespace xdb::ipc
