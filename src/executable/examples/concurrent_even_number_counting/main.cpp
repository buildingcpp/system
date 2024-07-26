#include <library/system.h>

#include <cstddef>
#include <iostream>
#include <atomic>
#include <vector>
#include <algorithm>
#include <mutex>
#include <condition_variable>


namespace bcpp::system
{

    template <typename result_type>
    class task
    {
    public:

        struct internals
        {
            std::mutex mutex_;
            std::condition_variable conditionVariable_;
            bool volatile done_{false};
            result_type result_;
        };

        task() = default;
        task(task &&) = default;

        task
        (
            std::invocable auto && work,
            blocking_work_contract_group & wcg
        )
        {
            internals_ = std::make_shared<internals>();
            wcg.create_contract(+[](){}, [internals = internals_, w = std::forward<decltype(work)>(work)]() mutable
                    {
                        internals->result_ = w();
                        internals->done_ = true;
                        std::unique_lock uniqueLock(internals->mutex_);
                        internals->conditionVariable_.notify_all();
                    });
        }

        auto join()
        {
            std::unique_lock uniqueLock(internals_->mutex_);
            internals_->conditionVariable_.wait(uniqueLock, [&](){return internals_->done_;});
            return internals_->result_;  
        }

    private:

        std::shared_ptr<internals> internals_;
    };


    auto create_task
    (
        std::invocable auto && work,
        blocking_work_contract_group & wcg
    )
    {
        return task<std::invoke_result_t<decltype(work)>>(work, wcg);
    }

}


namespace 
{
    auto make_vector
    (
    )
    {
        auto size = (1 << 20);
        std::vector<int> array(size);
        for (auto & v : array) 
            v = rand();    
        return array;    
    }
}


//=============================================================================
int main
(
    int argc,
    char ** args
)
{
    bcpp::system::blocking_work_contract_group workContractGroup(256);

    auto numWorkerThreads = std::thread::hardware_concurrency();
    std::vector<bcpp::system::thread_pool::thread_configuration> threadPoolConfiguration;
    for (auto i = 0; i < numWorkerThreads; ++i)
        threadPoolConfiguration.push_back({.function_ = [&](std::stop_token const & stopToken){while (!stopToken.stop_requested()) workContractGroup.execute_next_contract();}});
    bcpp::system::thread_pool threadPool(threadPoolConfiguration);

    auto array = make_vector();
    auto chunkSize = (array.size() / numWorkerThreads);  

    std::vector<bcpp::system::task<int>> tasks;
    for (auto cur = array.begin(); cur <= (array.end() - chunkSize); cur += chunkSize)
        tasks.push_back(bcpp::system::create_task(
                [beg = cur, end = cur + chunkSize]
                (
                ) mutable
                {
                    auto result = 0; 
                    while (beg < end) 
                        result += ((*(beg++) % 2) == 0);
                    return result;
                }, workContractGroup));

    int total{0};    
    for (auto & task : tasks)
        total += task.join();

    std::cout << "total = " << total << "\n";
    return 0;
}
