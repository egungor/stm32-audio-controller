// Thread-safe queue that serialises events into the single-threaded
// HostController (see HostController.h).
//
// Core Audio calls its notification interfaces on its own threads, often
// several times for one user action. Producers only push(); the consumer
// thread pops and calls the controller. A kind of event that is already
// waiting is not queued twice, so a burst of notifications becomes one
// refresh. Order between different kinds is kept.
#pragma once

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <optional>

namespace stm32_audio::host {

enum class HostEvent {
    DevicesChanged,  // endpoint added, removed, state or name changed
    DefaultChanged,  // default render endpoint changed
    VolumeChanged,   // volume or mute of the default endpoint changed
    Quit,
};

class EventQueue {
public:
    void push(HostEvent event)
    {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (HostEvent pending : events_) {
                if (pending == event) {
                    return;
                }
            }
            events_.push_back(event);
        }
        cv_.notify_one();
    }

    // Wait up to @p timeout for an event; nullopt on timeout.
    template <typename Rep, typename Period>
    std::optional<HostEvent> pop(std::chrono::duration<Rep, Period> timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [this] { return !events_.empty(); })) {
            return std::nullopt;
        }
        const HostEvent event = events_.front();
        events_.pop_front();
        return event;
    }

    std::size_t size() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return events_.size();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<HostEvent> events_;
};

}  // namespace stm32_audio::host
