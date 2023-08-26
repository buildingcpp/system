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
#include <span>

#include <library/system.h>
#include <tbb/concurrent_queue.h>
#include <concurrentqueue.h>

using namespace bcpp::system;


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


/*
//=============================================================================
void bare_minimum_example
(
    // bare minimum code 
    // actually pointless (in the real world) because there is no async processing.
    // however, a mimium example is useful for understading the basic concepts.
)
{
    work_contract_group workContractGroup(8);
    auto workContract = workContractGroup.create_contract([](){std::cout << "contract invoked\n";});
    workContract.invoke();
    workContractGroup.execute_next_contract();
}


//=============================================================================
void work_contract_after_group_destroyed_test
(
    // test.  destroy the owning work contract group PRIOR to the SURRENDER
    // of the work contract.  Note: INVOCATION of the work contract is UB once
    // the owning work contract group has been destroyed.  However, all work contracts
    // will invoke their surrender function when destroyed.  We want to insure that this
    // surrender FAILS in the case where the work contract group has already been destroyed.
)
{
    auto workContractGroup = std::make_unique<work_contract_group>(8);
    auto workContract = workContractGroup->create_contract([](){std::cout << "contract invoked\n";}, nullptr);

    workContract.invoke();
    workContractGroup->execute_next_contract();
    workContractGroup.reset();
}


//=============================================================================
void basic_example
(
    // create a work contract group
    // create a worker thread to asyncrhonously process invoked contracts
    // create a work contract
    // invoke the work contract some predetermined number of times
    // surrender the work contract
    // wait for the async surrender of the work contract to complete
)
{
    // create work contract group
    static auto constexpr max_number_of_contracts = 32;
    work_contract_group workContractGroup(max_number_of_contracts);

    // create worker thread to service work contracts asynchronously
    std::jthread workerThread([&](auto const & stopToken)
            {
                while (!stopToken.stop_requested()) 
                    workContractGroup.execute_next_contract();
            });

    std::atomic<std::size_t> invokeCounter{16};
    std::atomic<bool> surrendered{false};

    // create a work contract from the work contract group
    auto workContract = workContractGroup.create_contract(
            [&](){std::cout << "invokeCounter = " << --invokeCounter << "\n";},         // this is the async function
            [&](){std::cout << "work contract surrendered\n"; surrendered = true;});    // this is the one-shot surrender function
                    
    // invoke the work contract until the number of async invocations of that contract have been completed
    while (invokeCounter)
        workContract.invoke();

    // explicitly surrender the work contract.  this could be done by simply letting the work contract
    // go out of scope however, the surrender function is async and therefore might not take place.
    workContract.surrender();
    // wait for the work contract's one-shot surrender function to take place
    while (!surrendered)
        ;
}
*/

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

    std::vector<bcpp::system::thread_pool::thread_configuration> threads(numWorkerThreads);
    auto index = 0;
    int cores[] = {18,19,20,21,22,25,26,27,28,29,31};
    for (auto & thread : threads)
    {
        thread.cpuId_ = cores[index];
        thread.function_ = [&, threadIndex = index]
                (
                    auto const & stopToken
                ) mutable
                {
                    while (!stopToken.stop_requested())
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
    bcpp::system::thread_pool threadPool({.threads_ = threads});

    for (auto i = 0; i < numConcurrentTasks; ++i)
        queue.push({i, task});

    // start test
    auto startTime = std::chrono::system_clock::now();
    // wait for duration of test
    std::this_thread::sleep_for(testDuration);
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

    std::vector<bcpp::system::thread_pool::thread_configuration> threads(numWorkerThreads);
    auto index = 0;
    int cores[] = {18,19,20,21,22,25,26,27,28,29,31};
    for (auto & thread : threads)
    {
        thread.cpuId_ = cores[index];
        thread.function_ = [&, threadIndex = index]
                (
                    auto const & stopToken
                ) mutable
                {
                    while (!stopToken.stop_requested())
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
    bcpp::system::thread_pool threadPool({.threads_ = threads});

    for (auto i = 0; i < numConcurrentTasks; ++i)
        while (!queue.enqueue({i, task}))
            ;

    // start test
    auto startTime = std::chrono::system_clock::now();
    // wait for duration of test
    std::this_thread::sleep_for(testDuration);
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
    non_blocking_work_contract_group workContractGroup(maxTaskCapacity);
    std::vector<non_blocking_work_contract_group::work_contract_type> workContracts(numConcurrentTasks);

    // containers for gathering stats during test
    std::vector<std::size_t> perThreadTaskCount(numWorkerThreads);
    std::vector<std::size_t> taskExecutionCount(numConcurrentTasks);
    std::size_t thread_local tlsThreadIndex;

    // create work contracts
    for (auto i = 0; i < numConcurrentTasks; ++i)
    {
        workContracts[i] = workContractGroup.create_contract(
                [&, index = i]
                (
                    // the work to do.  
                    // each time work contract is executed increase counter and then re-invoke the same contract again.
                )
                {
                    ++perThreadTaskCount[tlsThreadIndex];
                    task();
                    taskExecutionCount[index]++;
                    workContracts[index].invoke();
                });
    }

    // invoke each of the contracts to start things off
    for (auto & workContract : workContracts)
        workContract.invoke();

    // create a worker thread pool and direct the threads to service the work contract group
    // ensure that each thread is on its own cpu for the sake of this test
    std::vector<bcpp::system::thread_pool::thread_configuration> threads(numWorkerThreads);
    auto index = 0;
    int cores[] = {18,19,20,21,22,25,26,27,28,29,31};

    for (auto i = 0; i < numWorkerThreads; ++i)
    {
        threads[i].cpuId_ = cores[i];
        threads[i].function_ = [&, threadIndex = i]
                (
                    auto const & stopToken
                ) mutable
                {
                    tlsThreadIndex = threadIndex;
                    while (!stopToken.stop_requested())
                        workContractGroup.execute_next_contract();
                };
    }
    bcpp::system::thread_pool threadPool({.threads_ = threads});

    // start test
    auto startTime = std::chrono::system_clock::now();
    // wait for duration of test
    std::this_thread::sleep_for(testDuration);

    // stop worker threads
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
  //  bare_minimum_example();
  //  basic_example();
  //  work_contract_after_group_destroyed_test();

    static auto constexpr testDuration = 10s;
    static auto constexpr numConcurrentTasks = 256;
    static auto constexpr maxTaskCapacity = 1024;

    auto run_test = []<typename T>
    (
        T task,
        std::string title
    )
    {
        static auto constexpr num_loops = 10;

        std::cout << "tbb queue\n";
        std::cout << "task = " << title << "\n";
        std::cout << "ops/s per thread, task mean, task std dev, task cv, thread std dev, thread cv\n";
        for (auto i = 0; i < num_loops; ++i)
            tbb_test(i + 1, testDuration, numConcurrentTasks, maxTaskCapacity, task);

        std::cout << "mpmc queue\n";
        std::cout << "task = " << title << "\n";
        std::cout << "ops/s per thread, task mean, task std dev, task cv, thread std dev, thread cv\n";
        for (auto i = 0; i < num_loops; ++i)
            mpmc_test(i + 1, testDuration, numConcurrentTasks, maxTaskCapacity, task);

        std::cout << "work contract\n";
        std::cout << "task = " << title << "\n";
        std::cout << "ops/s per thread, task mean, task std dev, task cv, thread std dev, thread cv\n";
        for (auto i = 0; i < num_loops; ++i)
            work_contract_test(i + 1, testDuration, numConcurrentTasks, maxTaskCapacity, task);

    };

    run_test(seive<100>, "heavy contention");
    run_test(seive<300>, "medium contention");
    run_test(seive<1000>, "low contention");
    run_test(+[](){}, "maximum contention");

    return 0;
}
