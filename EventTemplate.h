/**************************************************************

DESCRIPTION

	This file defines header of CEvent class.

**************************************************************/


#ifndef __EventTemplate_h__
#define __EventTemplate_h__

#include <functional>
#include <vector>
#include <algorithm>
#include <iostream>
#include <memory>

template <typename... Args>
class CSimpleEvent {
public:
    using Callback = std::function<void(Args...)>;

    // Simplified subscription: Just add the callback to the list
    void subscribe(Callback callback) {
        callbacks_.push_back(std::move(callback));
    }

    // Directly trigger all callbacks (no cleanup needed)
    void trigger(Args... args) {
        for (const auto& callback : callbacks_) {
            callback(args...);
        }
    }

private:
    std::vector<Callback> callbacks_;
};

template <typename... Args>
class CGlobalEvent {
public:
    using Callback = std::function<void(Args...)>;

    // Safe for one-time initialization
    void subscribe(Callback callback) {
        callbacks_.push_back(std::move(callback));
    }

    // Thread-safe for concurrent triggers after initialization
    void trigger(Args... args) {
        // 1. Ensure initialization is visible to all threads
        std::atomic_thread_fence(std::memory_order_acquire);

        // 2. Access callbacks without lock (read-only)
        for (const auto& callback : callbacks_) {
            if (callback) callback(args...);
        }
    }

private:
    std::vector<Callback> callbacks_;
};


template <typename... Args>
class CEvent : public std::enable_shared_from_this<CEvent<Args...>>  {
public:
    using Callback = std::function<void(Args...)>;

    class Subscription {
        friend class CEvent; // Grant Event access to private members
    public:
        // Destructor: Automatically unsubscribes when Subscription is destroyed
        ~Subscription() {
        	//if (event_) event_->unsubscribe(id_);
        	if (auto event = event_.lock()) {
				event->unsubscribe(id_);
			}
        }
        // Move constructor (transfer ownership)
        Subscription(Subscription&& other) noexcept : event_(other.event_), id_(other.id_) {
            other.event_.reset();// weak_prt // = nullptr; // Invalidate the moved-from object
            other.id_= -1; //invalidate
        }
        // Move assignment operator
        Subscription& operator=(Subscription&& other) noexcept {
            if (this != &other) {
                event_ = other.event_; // Copy the weak_ptr
                id_ = other.id_;
                other.event_.reset();// = nullptr;
                other.id_ = -1;         // Invalidate the source's ID
            }
            return *this;
        }
        // Disable copying (subscriptions are unique ownership)
        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;
    private:
        // Private constructor: Only Event can create Subscriptions
        Subscription(std::weak_ptr<CEvent> event, int id) //(Event* event, int id)
    		: event_(event), id_(id) {}
        std::weak_ptr<CEvent> event_; // Use weak_ptr instead of raw pointer: Event* event_ = nullptr;
        int id_;
    };

    //Subscription subscribe(Callback callback, const std::string& info) {
    Subscription subscribe(Callback callback) {
    	std::weak_ptr<CEvent> weakSelf = this->shared_from_this(); // Create a weak_ptr from the shared_ptr
        int id = nextId_++;
        //std::move transfers ownership of the callback from the subscribe parameter to the CallbackEntry
        //callbacks_.emplace_back(CallbackEntry{id, std::move(callback), info});
        //std::cout << "subscribe " << info.c_str() << ", id " << id << std::endl;
        callbacks_.emplace_back(CallbackEntry{id, std::move(callback)});
        return Subscription(weakSelf, id); //Subscription(this, id);
    }

    void trigger(Args... args) {
        // Clean up inactive entries before processing
        if (needsCleanup_) {
            callbacks_.erase(
                std::remove_if(callbacks_.begin(), callbacks_.end(),
                               [](const CallbackEntry& entry) { return !entry.active; }),
                callbacks_.end());
            needsCleanup_ = false;
        }

        for (const auto& entry : callbacks_) {
            entry.callback(args...);
        }
    }

private:
    struct CallbackEntry {
        int id;
        Callback callback;
        bool active;// {true};// = true; //active flag
        //std::string info; //debug info

        CallbackEntry(int id, Callback callback, bool active = true)
            : id(id), callback(std::move(callback)), active(active) {}
    };
    bool needsCleanup_ = false; //track cleanup state
    int nextId_ = 0;
    std::vector<CallbackEntry> callbacks_;

    void unsubscribe(int id) {
        auto it = std::find_if(callbacks_.begin(), callbacks_.end(),
                               [id](const CallbackEntry& entry) { return entry.id == id; });
        if (it != callbacks_.end()) {
        	//std::cout << "unsubscribe " << it->info.c_str() << ", id " << id << std::endl;
            //callbacks_.erase(it);
            it->active = false; // Mark as inactive instead of erasing
            needsCleanup_ = true;
        }
    }
};


#include <mutex>
#include <condition_variable>

template <typename... Args>
class CEventSafe : public std::enable_shared_from_this<CEventSafe<Args...>> {
public:
    using Callback = std::function<void(Args...)>;

    class Subscription {
        friend class CEventSafe;
    public:
        ~Subscription() {
            if (auto event = event_.lock()) {
                event->unsubscribe(id_);
            }
        }

        Subscription(Subscription&& other) noexcept
            : event_(std::move(other.event_)), id_(other.id_) {
            other.id_ = -1;
        }

        Subscription& operator=(Subscription&& other) noexcept {
            if (this != &other) {
                event_ = std::move(other.event_);
                id_ = other.id_;
                other.id_ = -1;
            }
            return *this;
        }

        Subscription(const Subscription&) = delete;
        Subscription& operator=(const Subscription&) = delete;

    private:
        Subscription(std::weak_ptr<CEventSafe> event, int id)
            : event_(std::move(event)), id_(id) {}

        std::weak_ptr<CEventSafe> event_;
        int id_ = -1;
    };

    Subscription subscribe(Callback callback) {
        std::weak_ptr<CEventSafe> weakSelf = this->shared_from_this();
        std::lock_guard<std::mutex> lock(mutex_);
        int id = nextId_++;
        auto entry = std::make_shared<CallbackEntry>(id, std::move(callback));
        callbacks_.push_back(entry);
        return Subscription(std::move(weakSelf), id);
    }

    void trigger(Args... args) {
        std::vector<std::shared_ptr<CallbackEntry>> activeEntries;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (needsCleanup_) {
                callbacks_.erase(
                    std::remove_if(callbacks_.begin(), callbacks_.end(),
                        [](const std::shared_ptr<CallbackEntry>& entry) {
                            return !entry->active;
                        }),
                    callbacks_.end());
                needsCleanup_ = false;
            }
            activeEntries.reserve(callbacks_.size());
            for (auto& entry : callbacks_) {
                if (entry->active) {
                    activeEntries.push_back(entry);
                }
            }
        }

        for (auto& entry : activeEntries) {
            entry->callback(args...);
        }
    }

private:
    struct CallbackEntry {
        int id;
        Callback callback;
        bool active;

        CallbackEntry(int id, Callback callback, bool active = true)
            : id(id), callback(std::move(callback)), active(active) {}
    };

    void unsubscribe(int id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = std::find_if(callbacks_.begin(), callbacks_.end(),
            [id](const std::shared_ptr<CallbackEntry>& entry) {
                return entry->id == id;
            });
        if (it != callbacks_.end()) {
            (*it)->active = false;
            needsCleanup_ = true;
        }
    }

    mutable std::mutex mutex_;
    bool needsCleanup_ = false;
    int nextId_ = 0;
    std::vector<std::shared_ptr<CallbackEntry>> callbacks_;
};

// usage example
/*
class CDevStatusHandler
{
    CGlobalEvent<std::string> m_onStatusToGuiUpdate;

public:
    CDevStatusHandler();
};

CDevStatusHandler::CDevStatusHandler()
{
    m_onStatusToGuiUpdate.subscribe([this](std::string info) {
        std::cout << "onStatusToGuiUpdate Event triggered: " << info;
    });
}

void CEventDev::SetState(int nState)
{
    if (m_nState == nState) {
        return;
    }
    m_nState = nState;

    char acLog[100];
    if (nState == DEV_STATE_OK) {
        snprintf(acLog, sizeof(acLog), "%s_%d Ok", m_acName, m_nId);
    } else {
        snprintf(acLog, sizeof(acLog), "%s_%d Down", m_acName, m_nId);
    }
    
    g_pLog->LogInfo(LOG_SYS, acLog);

    if (g_pDevStatus) {
        g_pDevStatus->m_onStatusToGuiUpdate.trigger(acLog);
    }
}
*/

#endif
