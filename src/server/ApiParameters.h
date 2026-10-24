#pragma once
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace xdb::server::detail {
inline std::string screenshot_format(const nlohmann::json &params) {
    auto format = params.value("format", std::string("png"));
    if (format != "png" && format != "bgra")
        throw std::invalid_argument("Screenshot format must be png or bgra");
    return format;
}
inline std::optional<int> recording_orientation(std::string_view orientation) {
    if (orientation == "portrait" || orientation == "rot0") return 0;
    if (orientation == "landscapeLeft" || orientation == "rot90") return 90;
    if (orientation == "portraitUpsideDown" || orientation == "rot180") return 180;
    if (orientation == "landscapeRight" || orientation == "rot270") return 270;
    return std::nullopt;
}
} // namespace xdb::server::detail
