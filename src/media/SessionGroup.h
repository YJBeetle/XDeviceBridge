#pragma once

#include <memory>
#include <mutex>
#include <utility>

namespace xdb::media {

// CoreDevice exposes stopAll rather than a per-stream stop. Register a pump before
// starting its device RPC, and serialize registration with stopAll so a concurrent
// new stream cannot be stopped between an ownership check and the cleanup RPC.
class SessionGroup {
    std::mutex mutex_;
    unsigned users_ = 0;
public:
    class Lease {
        std::shared_ptr<SessionGroup> group_;
    public:
        explicit Lease(std::shared_ptr<SessionGroup> group) : group_(std::move(group)) {
            if (group_) { std::lock_guard lock(group_->mutex_); ++group_->users_; }
        }
        ~Lease() { release(); }
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        void release() {
            if (group_) {
                auto group = std::move(group_);
                std::lock_guard lock(group->mutex_);
                --group->users_;
            }
        }
        // With another pump alive, stop its sibling locally and let the device's
        // unrenewed stream expire. Skipping stopAll is a successful local stop.
        template<class Stop> bool stop_if_exclusive(Stop stop) const {
            if (!group_) return stop();
            std::lock_guard lock(group_->mutex_);
            return group_->users_ != 1 || stop();
        }
    };
};
} // namespace xdb::media
