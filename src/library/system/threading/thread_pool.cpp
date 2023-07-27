#include "./thread_pool.h"

#include <library/system.h>


//=============================================================================
bcpp::system::thread_pool::thread_pool
(
    configuration const & config
):
    threads_(config.threads_.size())
{
    auto index = 0;
    for (auto & thread : threads_)
    {
        thread = std::jthread([config = config.threads_[index]]
                (
                    std::stop_token stopToken
                )
                {
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
