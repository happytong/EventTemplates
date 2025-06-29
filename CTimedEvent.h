
// This code defines a C++ class `CTimedEvent` that allows you to subscribe to events with immediate or delayed callbacks.
#include <functional>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>

int delay(int nMs) {
    if (nMs < 0) return -1;
    struct timespec requested = {
        .tv_sec = static_cast<time_t>(nMs / 1000),
        .tv_nsec = static_cast<long>((nMs % 1000) * 1000000L)
    };
    while (nanosleep(&requested, &requested) == -1 && errno == EINTR);
    return 0;
}

template <typename... Args>
class CTimedEvent {
private:
    struct TimedCallback {
        std::function<void(Args...)> func;
        unsigned int delay_ms;
    };

    std::vector<std::function<void(Args...)>> immediate_callbacks_;
    std::vector<TimedCallback> delayed_callbacks_;
    std::mutex mutex_;

    // Helper to launch delayed callback
    void launch_delayed(const std::function<void(Args...)> callback,
                       unsigned int delay_ms,
                       Args... args) {
        // Bind the callback with its arguments
        auto bound_func = std::bind(callback, args...);
        std::thread([bound_func, delay_ms]() {
            delay(delay_ms);  // Custom delay function
            bound_func();     // Invoke the bound function
        }).detach();
    }

public:
    using Callback = std::function<void(Args...)>;

    void subscribe(Callback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        immediate_callbacks_.push_back(std::move(callback));
    }

    void subscribe_with_delay(Callback callback, unsigned int delay_ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        delayed_callbacks_.push_back({std::move(callback), delay_ms});
    }

    void trigger(Args... args) {

        // Process immediate callbacks
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& cb : immediate_callbacks_) {
                if (cb) cb(args...);
            }
        }

        // Process delayed callbacks
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& tcb : delayed_callbacks_) {
                if (tcb.func) {
                    launch_delayed(tcb.func, tcb.delay_ms, args...);
                }
            }
        }
    }
};