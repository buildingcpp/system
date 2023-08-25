#include <cstddef>
#include <iostream>
#include <memory>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <cstdint>
#include <library/system.h>
#include <atomic>
#include <vector>
#include <thread>
#include <cmath>
#include <iomanip>
#include <library/system.h>

#include <concurrentqueue.h>


//=============================================================================
std::int32_t work_function
(
)
{
    auto t = 0;
    for (auto i = 0; i < (1 << 8); ++i)
        t += std::to_string(i).size();
    return t;
};


//=============================================================================
auto mpmc_test
(
    std::size_t num_worker_threads
)
{
    static auto constexpr test_duration = std::chrono::milliseconds(10000);
    static auto constexpr num_tasks = (1 << 8);
    using work_pair = std::pair<std::int32_t, std::int32_t(*)()>;
    moodycamel::ConcurrentQueue<work_pair> queue(num_tasks);

    std::vector<std::int64_t> totalTaskCount(num_tasks);
    auto useless = 0;

    std::vector<bcpp::system::thread_pool::thread_configuration> threads(num_worker_threads);
    auto index = 0;
    for (auto & thread : threads)
    {
        thread.cpuId_ = index * 2;
        thread.function_ = [&]
                (
                    auto const & stopToken
                ) mutable
                {
                    while (!stopToken.stop_requested())
                    {
                        work_pair p;
                        if (queue.try_dequeue(p))
                        {
                            useless += p.second();
                            totalTaskCount[p.first]++;
                            queue.enqueue(p);
                        }
                    }
                };
        ++index;
    }
    bcpp::system::thread_pool threadPool({.threads_ = threads});

    for (auto i = 0; i < num_tasks; ++i)
        while (!queue.enqueue({i, work_function}))
            ;

    // start test
    auto startTime = std::chrono::system_clock::now();
    // wait for duration of test
    std::this_thread::sleep_for(test_duration);
    // stop worker threads
    threadPool.stop(bcpp::system::synchronization_mode::async);
    // wait for all threads to exit
    threadPool.wait_stop_complete();

    // test completed
    // gather timing
    auto stopTime = std::chrono::system_clock::now();
    auto elapsedTime = (stopTime - startTime);
    auto test_duration_in_sec = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(elapsedTime).count() / std::nano::den;
    // calculate std deviation etc
    std::size_t n = 0;    
    for (auto i = 0; i < num_tasks; ++i)
        n += totalTaskCount[i];
    long double m = ((long double)n / num_tasks);
    long double k = 0;
    for (auto i = 0; i < num_tasks; ++i)
        k += ((totalTaskCount[i] - m) * (totalTaskCount[i] - m));
    k /= (num_tasks - 1);
    auto sd = std::sqrt(k);
    // report results
    std::cout << "Threads = " << num_worker_threads << ", total tasks = " << n << " , tasks per sec = " << (int)(n / test_duration_in_sec) << 
            " , tasks per thread per sec = " << (int)((n / test_duration_in_sec) / num_worker_threads) << 
            " , mean = " << m << " , std dev = " << sd << " , cv = " << std::fixed << std::setprecision(3) << (sd / m) << std::endl;
    return useless;
}


//=============================================================================
int main
(
    int, 
    char const **
)
{
    static auto constexpr num_loops = 10;
    for (auto i = 0; i < num_loops; ++i)
        mpmc_test(i + 1);
    return 0;
}
