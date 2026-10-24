#pragma once
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <stop_token>
#include <string>
#include <vector>

namespace xdb::hid::detail {
// Delivery remains asynchronous at the phone; stop prevents further local reports.
inline bool send_text_reports(const std::vector<std::vector<uint16_t>> &reports, int hold_ms,
                              std::stop_token stop,
                              const std::function<bool(const std::vector<uint16_t> &)> &send,
                              std::string &error) {
    std::mutex mutex;
    std::condition_variable_any wake;
    for (const auto &report : reports) {
        if (stop.stop_requested()) {
            error = "Text input cancelled";
            return false;
        }
        if (!send(report)) return false;
        if (hold_ms > 0) {
            std::unique_lock lock(mutex);
            wake.wait_for(lock, stop, std::chrono::milliseconds(hold_ms), [] { return false; });
        }
    }
    if (stop.stop_requested()) {
        error = "Text input cancelled";
        return false;
    }
    return true;
}
} // namespace xdb::hid::detail
