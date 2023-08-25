#include "./waitable_state.h"

/*
//=============================================================================
void bcpp::system::waitable_state::wait_for
(
    std::chrono::nanoseconds duration
) const
{
    std::unique_lock uniqueLock(mutex_);
    conditionVariable_.wait_for(uniqueLock, duration, [this](){return ((activeCount_ > 0) || (stopped_));});
}
*/