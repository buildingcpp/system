#pragma once

#include <include/non_movable.h>
#include <include/non_copyable.h>

#include <memory>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <condition_variable>


namespace bcpp::system 
{

    class waitable_state :
        non_copyable,
        non_movable
    {
    public:

        void increment_activity_count();

        void decrement_activity_count();

        bool wait_for
        (
            std::chrono::nanoseconds
        ) const;

        void stop();

    private:

        std::mutex mutable              mutex_;
        std::condition_variable mutable conditionVariable_;
        std::atomic<std::size_t>        activeCount_;
        std::atomic<bool>               stopped_{false};
    };

} // namespace bcpp::system


//=============================================================================
inline void bcpp::system::waitable_state::increment_activity_count
(
)
{
    if (++activeCount_ == 1)
    {
        std::lock_guard lockGuard(mutex_);
        conditionVariable_.notify_all();
    }
}


//=============================================================================
inline void bcpp::system::waitable_state::decrement_activity_count
(
)
{
    --activeCount_;
}


//=============================================================================
inline bool bcpp::system::waitable_state::wait_for
(
    std::chrono::nanoseconds duration
) const
{
    if (activeCount_ == 0)
    {
        std::unique_lock uniqueLock(mutex_);
        return conditionVariable_.wait_for(uniqueLock, duration, [this](){return ((activeCount_ > 0) || (stopped_));});
    }
    return true;
}


//=============================================================================
inline void bcpp::system::waitable_state::stop
(
)
{
    if (bool wasRunning = !stopped_.exchange(true); wasRunning)
    {
        std::unique_lock uniqueLock(mutex_);
        conditionVariable_.notify_all();
    }
}
