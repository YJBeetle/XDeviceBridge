#pragma once

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace xdb::util {
// Shared by the CLI and low-level pairing defaults; explicit server/app paths bypass this.
inline std::string default_data_directory() {
    const char *base = nullptr;
    std::filesystem::path suffix;
#ifdef _WIN32
    base = std::getenv("LOCALAPPDATA");
    suffix = "XDeviceBridge";
#elif defined(__APPLE__)
    base = std::getenv("HOME");
    suffix = std::filesystem::path("Library") / "Application Support" / "XDeviceBridge";
#else
    base = std::getenv("XDG_DATA_HOME");
    suffix = "XDeviceBridge";
    if (!base || !*base) {
        base = std::getenv("HOME");
        suffix = std::filesystem::path(".local") / "share" / "XDeviceBridge";
    }
#endif
    if (!base || !*base)
        throw std::invalid_argument("Cannot locate user data directory; supply --data-dir");
    const auto root = std::u8string(base, base + std::char_traits<char>::length(base));
    const auto path = (std::filesystem::path(root) / suffix).u8string();
    return {path.begin(), path.end()};
}
} // namespace xdb::util
