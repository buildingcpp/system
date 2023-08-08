#include "./thread_pool.h"

#include <library/system.h>


//=============================================================================
bcpp::system::thread_pool::thread_pool
(
    configuration const & config
):
    threads_(config.threads_.size()),
    threadCount_(std::make_shared<std::atomic<std::size_t>>(0)),
    conditionVariable_(std::make_shared<std::condition_variable>())
{
    auto index = 0;
    for (auto & thread : threads_)
    {
        thread = std::jthread([config = config.threads_[index], threadCount = threadCount_, conditionVariable = conditionVariable_]
                (
                    std::stop_token stopToken
                )
                {
                    try
                    {
                        if (config.cpuId_.has_value())
                            set_cpu_affinity(config.cpuId_.value());
                        ++(*threadCount);
                        if (config.initializeHandler_)
                            config.initializeHandler_();
                        config.function_(stopToken);
                        if (config.terminateHandler_)
                            config.terminateHandler_();
                    }
                    catch (...)
                    {
                        auto currentException = std::current_exception();
                        if (config.exceptionHandler_)
                            config.exceptionHandler_(currentException);
                        else
                            std::rethrow_exception(currentException);
                    }
                    if (--(*threadCount) == 0)
                        conditionVariable->notify_all();
                });
        ++index;
    }
}


//=============================================================================
void bcpp::system::thread_pool::stop
(
    // issue terminate to all worker threads
)
{
    stop(synchronization_mode::async);
}


//=============================================================================
void bcpp::system::thread_pool::stop
(
    // issue terminate to all worker threads. 
    // waits for all threads to terminate if waitForTerminationComplete is true
    synchronization_mode stopMode
)
{       
    for (auto & thread : threads_)
        thread.request_stop();

    if (stopMode == synchronization_mode::blocking)
        for (auto & thread : threads_)
            if (thread.joinable())
                thread.join();
}


//=============================================================================
bool bcpp::system::thread_pool::wait_stop_complete
(
    std::chrono::nanoseconds duration
) const
{
    std::unique_lock uniqueLock(mutex_);
    return conditionVariable_->wait_for(uniqueLock, duration, [this](){return ((*threadCount_) == 0);});
}


//=============================================================================
void bcpp::system::thread_pool::wait_stop_complete
(
) const
{
    std::unique_lock uniqueLock(mutex_);
    return conditionVariable_->wait(uniqueLock, [this](){return ((*threadCount_) == 0);});
}
