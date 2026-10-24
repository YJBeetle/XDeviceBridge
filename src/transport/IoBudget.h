#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "transport/Socket.h"

namespace xdb::transport {

/// cancelled may be called by a scoped watchdog; it must be thread-safe.
struct StartupOptions {
    std::chrono::milliseconds timeout{30000};
    std::function<bool()> cancelled;
};

/// One absolute deadline shared by all stages and address candidates.
class IoBudget {
public:
    using Clock = std::chrono::steady_clock;
    explicit IoBudget(const StartupOptions &options = {})
        : deadline_(Clock::now() + std::clamp(options.timeout, std::chrono::milliseconds(0),
                                             std::chrono::milliseconds(std::chrono::hours(1)))),
          cancelled_(options.cancelled) {}

    bool stopped() const noexcept {
        if (reason_.load() != Reason::none) return true;
        Reason reason = Reason::none;
        try {
            if (cancelled_ && cancelled_()) reason = Reason::cancelled;
            else if (Clock::now() >= deadline_) reason = Reason::timed_out;
        } catch (...) { reason = Reason::callback_failed; }
        if (reason != Reason::none) {
            Reason expected = Reason::none;
            reason_.compare_exchange_strong(expected, reason);
        }
        return reason_.load() != Reason::none;
    }

    bool check(std::string &error, std::string_view operation = "Device startup") const {
        if (!stopped()) return true;
        switch (reason_.load()) {
        case Reason::cancelled: error = std::string(operation) + " cancelled"; break;
        case Reason::timed_out: error = std::string(operation) + " timed out"; break;
        case Reason::callback_failed: error = std::string(operation) + " cancellation callback failed"; break;
        case Reason::none: break;
        }
        return false;
    }

    void callback_failed() const noexcept { reason_ = Reason::callback_failed; }

    std::chrono::milliseconds remaining() const {
        if (stopped()) return std::chrono::milliseconds(0);
        return std::max(std::chrono::milliseconds(0),
                        std::chrono::ceil<std::chrono::milliseconds>(deadline_ - Clock::now()));
    }

private:
    enum class Reason { none, cancelled, timed_out, callback_failed };
    Clock::time_point deadline_;
    std::function<bool()> cancelled_;
    mutable std::atomic<Reason> reason_{Reason::none};
};

/// Interrupt blocking startup work when its budget stops. Join before moving
/// or destroying any object borrowed by the interrupt callback.
class BudgetWatchdog {
public:
    BudgetWatchdog(const IoBudget &budget, std::function<void()> interrupt)
        : budget_(budget), interrupt_(std::move(interrupt)) {
        if (budget_.stopped()) this->interrupt();
        else worker_ = std::thread([this] {
            std::unique_lock lock(mutex_);
            while (!done_) {
                cv_.wait_for(lock, std::min(budget_.remaining(), std::chrono::milliseconds(25)),
                             [this] { return done_; });
                if (done_) break;
                lock.unlock();
                const bool stopped = budget_.stopped();
                if (stopped) this->interrupt();
                lock.lock();
                if (stopped) break;
            }
        });
    }
    ~BudgetWatchdog() { stop(); }
    bool active() const { return !budget_.stopped(); }
    bool finish(std::string &error, std::string_view operation = "Device startup") {
        stop(); return budget_.check(error, operation);
    }
private:
    void interrupt() noexcept {
        try { interrupt_(); } catch (...) { budget_.callback_failed(); }
    }
    void stop() {
        { std::lock_guard lock(mutex_); done_ = true; }
        cv_.notify_all();
        if (worker_.joinable()) worker_.join();
    }
    const IoBudget &budget_;
    std::function<void()> interrupt_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool done_ = false;
    std::thread worker_;
};

class SocketBudgetGuard {
public:
    SocketBudgetGuard(Socket &socket, const IoBudget &budget)
        : watch_(budget, [&socket] { socket.interrupt(); }) {}
    bool finish(std::string &error) { return watch_.finish(error); }
private:
    BudgetWatchdog watch_;
};

} // namespace xdb::transport
