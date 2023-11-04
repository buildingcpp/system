#include <cstddef>
#include <iostream>
#include <memory>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <cstdint>
#include <atomic>
#include <vector>
#include <thread>
#include <cmath>
#include <iomanip>
#include <span>

#include <library/system.h>
#include <tbb/concurrent_queue.h>
#include <concurrentqueue.h>

using namespace bcpp::system;


// it might look a bit odd to hard code the cpus to use in the benchmark
// but one of my test machines has a blend of different cpus and I can't seem
// to disable hyperthreading on that machine via the bios nor the terminal.
// This made ensuring that the benchmark is running on the preferred physical cores 
// a bit difficult so I just did it this way until I have time to write a tool
// to dynamically figure out the optimal cores to use.
int cores[] = {0,2,4,6,8,10,12,14,16,18,20};


auto gather_stats
(
    std::span<std::size_t> const input
) -> std::tuple<std::size_t, long double, long double, long double>
{
    std::size_t total = 0;    
    for (auto v : input)
        total += v;
    long double mean = ((long double)total / input.size());
    long double k = 0;
    for (auto v : input)
        k += ((v - mean) * (v - mean));
    k /= (input.size() - 1);
    auto sd = std::sqrt(k);
    return {total, mean, sd, sd/ mean};
}


//=============================================================================
template <std::size_t N>
std::int32_t seive
(
)
{
    static auto constexpr max = N;
    bool seive[max];
    for (auto & _ : seive)
        _ = true;
    seive[0] = seive[1] = false;
    for (auto i = 2; i < max; ++i)
        for (auto n = i * 2; n < max; n += i)
            seive[n] = false;
    auto total = 0;
    for (auto n : seive)
        total += (n == true);
    return total;
};

//=============================================================================
template <typename T>
auto tbb_test
(
    std::size_t numWorkerThreads,
    std::chrono::nanoseconds testDuration,
    std::size_t numConcurrentTasks,
    std::size_t maxTaskCapacity,
    T task
)
{
    using work_pair = std::pair<std::int32_t, T>;
    tbb::concurrent_queue<work_pair> queue;

    // containers for gathering stats during test
    std::vector<std::size_t> perThreadTaskCount(numWorkerThreads);
    std::vector<std::size_t> taskExecutionCount(numConcurrentTasks);
    std::size_t thread_local tlsThreadIndex;
    bool volatile start = false;
    bool volatile end = false;

    std::vector<bcpp::system::thread_pool::thread_configuration> threads(numWorkerThreads);
    auto index = 0;
    for (auto & thread : threads)
    {
        thread.cpuId_ = cores[index];
        thread.function_ = [&, threadIndex = index]
                (
                    auto const & stopToken
                ) mutable
                {
                    while ((!stopToken.stop_requested()) && (!start))
                        ;
                    while ((!stopToken.stop_requested()) && (!end))
                    {
                        work_pair p;
                        if (queue.try_pop(p))
                        {
                            p.second();
                            taskExecutionCount[p.first]++;
                            perThreadTaskCount[threadIndex]++;
                            queue.push(p);
                        }
                    }
                };
        ++index;
    }
    bcpp::system::thread_pool threadPool(threads);

    for (auto i = 0; i < numConcurrentTasks; ++i)
        queue.push({i, task});

    // start test
    auto startTime = std::chrono::system_clock::now();
    start = true;
    // wait for duration of test
    std::this_thread::sleep_for(testDuration);
    end = true;
    // stop worker threads
    threadPool.stop(bcpp::system::synchronization_mode::async);
    // wait for all threads to exit
    threadPool.wait_stop_complete();

    // test completed
    // gather timing
    auto stopTime = std::chrono::system_clock::now();
    auto elapsedTime = (stopTime - startTime);
    auto test_duration_in_sec = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(elapsedTime).count() / std::nano::den;

    auto [taskTotal, taskMean, taskSd, taskCv] = gather_stats(taskExecutionCount);
    auto [threadTotal, threadMean, threadSd, threadCv] = gather_stats(perThreadTaskCount);

    std::cout << (int)((taskTotal / test_duration_in_sec) / numWorkerThreads) << ", " << taskMean << "," << taskSd << ", " << 
            std::fixed << std::setprecision(3) << taskCv << ", " << threadSd << ", " << threadCv << "\n";
}


//=============================================================================
template <typename T>
auto mpmc_test
(
    std::size_t numWorkerThreads,
    std::chrono::nanoseconds testDuration,
    std::size_t numConcurrentTasks,
    std::size_t maxTaskCapacity,
    T task
)
{
    using work_pair = std::pair<std::int32_t, T>;
    moodycamel::ConcurrentQueue<work_pair> queue(maxTaskCapacity);

    // containers for gathering stats during test
    std::vector<std::size_t> perThreadTaskCount(numWorkerThreads);
    std::vector<std::size_t> taskExecutionCount(numConcurrentTasks);
    std::size_t thread_local tlsThreadIndex;
    bool volatile start = false;
    bool volatile end = false;

    std::vector<bcpp::system::thread_pool::thread_configuration> threads(numWorkerThreads);
    auto index = 0;
    for (auto & thread : threads)
    {
        thread.cpuId_ = cores[index];
        thread.function_ = [&, threadIndex = index]
                (
                    auto const & stopToken
                ) mutable
                {                    
                    while ((!stopToken.stop_requested()) && (!start))
                        ;
                    while ((!stopToken.stop_requested()) && (!end))
                    {
                        work_pair p;
                        if (queue.try_dequeue(p))
                        {
                            p.second();
                            taskExecutionCount[p.first]++;
                            perThreadTaskCount[threadIndex]++;
                            while (!queue.enqueue(p))
                                ;
                        }
                    }
                };
        ++index;
    }
    bcpp::system::thread_pool threadPool(threads);

    for (auto i = 0; i < numConcurrentTasks; ++i)
        while (!queue.enqueue({i, task}))
            ;

    // start test
    auto startTime = std::chrono::system_clock::now();
    start = true;
    // wait for duration of test
    std::this_thread::sleep_for(testDuration);
    end = true;
    // stop worker threads
    threadPool.stop(bcpp::system::synchronization_mode::async);
    // wait for all threads to exit
    threadPool.wait_stop_complete();

    // test completed
    // gather timing
    auto stopTime = std::chrono::system_clock::now();
    auto elapsedTime = (stopTime - startTime);
    auto test_duration_in_sec = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(elapsedTime).count() / std::nano::den;

    auto [taskTotal, taskMean, taskSd, taskCv] = gather_stats(taskExecutionCount);
    auto [threadTotal, threadMean, threadSd, threadCv] = gather_stats(perThreadTaskCount);

    std::cout << (int)((taskTotal / test_duration_in_sec) / numWorkerThreads) << ", " << taskMean << "," << taskSd << ", " << 
            std::fixed << std::setprecision(3) << taskCv << ", " << threadSd << ", " << threadCv << "\n";
}


//=============================================================================
template <typename W, typename T>
auto work_contract_test
(
    std::size_t numWorkerThreads,
    std::chrono::nanoseconds testDuration,
    std::size_t numConcurrentTasks,
    std::size_t maxTaskCapacity,
    T task
)
{
    // contruct work contract group
    W workContractGroup(maxTaskCapacity);
    std::vector<typename W::work_contract_type> workContracts(numConcurrentTasks);

    // containers for gathering stats during test
    std::vector<std::size_t> perThreadTaskCount(numWorkerThreads);
    std::vector<std::size_t> taskExecutionCount(numConcurrentTasks);
    std::size_t thread_local tlsThreadIndex;
    bool volatile start = false;
    bool volatile end = false;

    // create work contracts
    for (auto i = 0; i < numConcurrentTasks; ++i)
    {
        workContracts[i] = workContractGroup.create_contract(
                [&, index = i]
                (
                    // the work to do.  
                    // each time work contract is executed increase counter and then re-schedule the same contract again.
                )
                {
                    ++perThreadTaskCount[tlsThreadIndex];
                    task();
                    taskExecutionCount[index]++;
                    workContracts[index].schedule();
                });
    }

    // schedule each of the contracts to start things off
    for (auto & workContract : workContracts)
        workContract.schedule();

    // create a worker thread pool and direct the threads to service the work contract group
    // ensure that each thread is on its own cpu for the sake of this test
    std::vector<bcpp::system::thread_pool::thread_configuration> threads(numWorkerThreads);
    auto index = 0;

    for (auto i = 0; i < numWorkerThreads; ++i)
    {
        threads[i].cpuId_ = cores[i];
        threads[i].function_ = [&, threadIndex = i]
                (
                    auto const & stopToken
                ) mutable
                {
                    tlsThreadIndex = threadIndex;
                    while ((!stopToken.stop_requested()) && (!start))
                        ;
                    while ((!stopToken.stop_requested()) && (!end))
                    {
                        if constexpr (std::is_same_v<W, blocking_work_contract_group>)
                            workContractGroup.execute_next_contract(std::chrono::milliseconds(1));
                        else
                            workContractGroup.execute_next_contract();
                    }
                };
    }
    bcpp::system::thread_pool threadPool(threads);

    // start test
    auto startTime = std::chrono::system_clock::now();
    start = true;
    // wait for duration of test
    std::this_thread::sleep_for(testDuration);

    // stop worker threads
    end = true;
    threadPool.stop(synchronization_mode::async);
    // wait for all threads to exit
    threadPool.wait_stop_complete();
    workContractGroup.stop();

    // test completed
    // gather timing
    auto stopTime = std::chrono::system_clock::now();
    auto elapsedTime = (stopTime - startTime);
    auto test_duration_in_sec = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(elapsedTime).count() / std::nano::den;

    auto [taskTotal, taskMean, taskSd, taskCv] = gather_stats(taskExecutionCount);
    auto [threadTotal, threadMean, threadSd, threadCv] = gather_stats(perThreadTaskCount);

    std::cout << (int)((taskTotal / test_duration_in_sec) / numWorkerThreads) << ", " << taskMean << "," << taskSd << ", " << 
            std::fixed << std::setprecision(3) << taskCv << ", " << threadSd << ", " << threadCv << "\n";
}


//=============================================================================
int main
(
    int, 
    char const **
)
{   
    using namespace std::chrono;

    static auto constexpr testDuration = 1s;
    static auto constexpr numConcurrentTasks = 1 << 8;
    static auto constexpr maxTaskCapacity = 1 << 8;

    auto run_test = []<typename T>
    (
        T task,
        std::string title
    )
    {
        static auto constexpr max_threads = 10;
/*
        std::cout << "TBB concurrent_queue\n";
        std::cout << "task = " << title << "\n";
        std::cout << "ops/s per thread, task mean, task std dev, task cv, thread std dev, thread cv\n";
        for (auto i = 2; i <= max_threads; ++i)
            tbb_test(i, testDuration, numConcurrentTasks, maxTaskCapacity, task);

        std::cout << "moodycamel ConcurrentQueue\n";
        std::cout << "task = " << title << "\n";
        std::cout << "ops/s per thread, task mean, task std dev, task cv, thread std dev, thread cv\n";
        for (auto i = 2; i <= max_threads; ++i)
            mpmc_test(i, testDuration, numConcurrentTasks, maxTaskCapacity, task);
*/
        std::cout << "work contract\n";
        std::cout << "task = " << title << "\n";
        std::cout << "ops/s per thread, task mean, task std dev, task cv, thread std dev, thread cv\n";
        for (auto i = 2; i <= max_threads; ++i)
            work_contract_test<work_contract_group>(i, testDuration, numConcurrentTasks, maxTaskCapacity, task);

        std::cout << "blocking work contract\n";
        std::cout << "task = " << title << "\n";
        std::cout << "ops/s per thread, task mean, task std dev, task cv, thread std dev, thread cv\n";
        for (auto i = 2; i <= max_threads; ++i)
            work_contract_test<blocking_work_contract_group>(i, testDuration, numConcurrentTasks, maxTaskCapacity, task);
    };

    run_test(+[](){}, "maximum contention");
    run_test(seive<100>, "high contention");
    run_test(seive<300>, "medium contention");
    run_test(seive<1000>, "low contention");

    return 0;
}


