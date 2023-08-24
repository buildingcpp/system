#include "./thread_pool.h"

#include <library/system.h>


namespace 
{
    struct thread_counter
    {
        thread_counter
        (
            std::shared_ptr<std::atomic<std::size_t>> counter,
            std::shared_ptr<std::mutex> mutex,
            std::shared_ptr<std::condition_variable> conditionVariable
        ):
            counter_(counter), 
            mutex_(mutex), 
            conditionVariable_(conditionVariable)
        {
            ++(*counter_);
        }


        ~thread_counter()
        {
            if (--(*counter_) == 0)
            {
                std::lock_guard lockGuard(*mutex_);
                conditionVariable_->notify_all();
            }
        }

        std::shared_ptr<std::atomic<std::size_t>> counter_;
        std::shared_ptr<std::mutex> mutex_;
        std::shared_ptr<std::condition_variable> conditionVariable_;
    };
}


//=============================================================================
bcpp::system::thread_pool::thread_pool
(
    configuration const & config
):
    threads_(config.threads_.size()),
    threadCount_(std::make_shared<std::atomic<std::size_t>>(0)),
    mutex_(std::make_shared<std::mutex>()),
    conditionVariable_(std::make_shared<std::condition_variable>())
{
    auto index = 0;
    for (auto & thread : threads_)
    {
        thread = std::jthread([config = config.threads_[index], threadCount = threadCount_, mutex = mutex_, conditionVariable = conditionVariable_]
                (
                    std::stop_token stopToken
                )
                {
                    thread_counter threadCounter(threadCount, mutex, conditionVariable);
                    try
                    {
                        if (config.cpuId_.has_value())
                            set_cpu_affinity(config.cpuId_.value());
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
    std::unique_lock uniqueLock(*mutex_);
    return conditionVariable_->wait_for(uniqueLock, duration, [this](){return ((*threadCount_) == 0);});
}


//=============================================================================
void bcpp::system::thread_pool::wait_stop_complete
(
) const
{
    std::unique_lock uniqueLock(*mutex_);
    return conditionVariable_->wait(uniqueLock, [this](){return ((*threadCount_) == 0);});
}
