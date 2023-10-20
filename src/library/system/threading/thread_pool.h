#pragma once

#include <include/non_copyable.h>
#include <library/system/cpu_id.h>
#include <include/synchronization_mode.h>

#include <exception>
#include <stdexcept>
#include <thread>
#include <vector>
#include <cstdint>
#include <functional>
#include <optional>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <memory>
#include <atomic>


namespace bcpp::system 
{
    
    class thread_pool :
        non_copyable
    {
    public:

        struct thread_configuration
        {
            std::function<void()>                           initializeHandler_;
            std::function<void()>                           terminateHandler_;
            std::function<void(std::exception_ptr)>         exceptionHandler_; 
            std::function<void(std::stop_token const &)>    function_;
            std::optional<cpu_id>                           cpuId_;
        };

        thread_pool() = default;
        
        thread_pool
        (
            std::vector<thread_configuration> const &
        );

        void stop();

        void stop(synchronization_mode);

        bool wait_stop_complete
        (
            std::chrono::nanoseconds
        ) const;

        void wait_stop_complete() const;

    private:
    
        std::vector<std::jthread>   threads_;

        std::shared_ptr<std::atomic<std::size_t>>   threadCount_;

        std::shared_ptr<std::mutex>                 mutex_;
        std::shared_ptr<std::condition_variable>    conditionVariable_;
    };
    
} // namespace bcpp::system 
