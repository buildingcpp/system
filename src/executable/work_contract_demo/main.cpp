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

#include <library/system.h>


//=============================================================================
void bare_minimum_example
(
    // bare minimum code 
    // actually pointless (in the real world) because there is no async processing.
    // however, a mimium example is useful for understading the basic concepts.
)
{
    bcpp::system::work_contract_group workContractGroup(8);
    auto workContract = workContractGroup.create_contract([](){std::cout << "contract invoked\n";}, nullptr);

    workContract.invoke();
    workContractGroup.service_contracts();
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
    bcpp::system::work_contract_group workContractGroup(max_number_of_contracts);

    // create worker thread to service work contracts asynchronously
    std::jthread workerThread([&](auto const & stopToken)
            {
                while (!stopToken.stop_requested()) 
                    workContractGroup.service_contracts();
            });

    std::atomic<std::size_t> invokeCounter{16};
    std::atomic<bool> surrendered{false};

    // create a work contract from the work contract group
    bcpp::system::work_contract workContract = workContractGroup.create_contract(
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


//=============================================================================
void multithreaded_example
(
    // measure performance where max number of contracts is large and where
    // there are always some preconfigured number of contracts invoked.
)
{
    static auto const num_worker_threads = std::thread::hardware_concurrency() / 2;
    static auto constexpr test_duration = std::chrono::milliseconds(1000);
    static auto constexpr max_contracts = (1 << 20);
    static auto constexpr max_concurrent_contracts = 32;
    static std::vector<std::size_t> contractId;

    static auto once = [&]()
            {
                for (auto i = 0; i < max_contracts; ++i)
                    contractId.push_back(i);
                for (auto i = 0; i < max_contracts; ++i)
                {
                    std::uint64_t j = ((rand() * rand()) % contractId.size());
                    while (j == i)
                        j = (((j * rand()) + 1) % contractId.size());
                    std::swap(contractId[i], contractId[j]);
                }
                return true;
            }();

    // enable each contract to invoke the next random contract upon its own completion.
    bcpp::system::work_contract_group workContractGroup(max_contracts);
    std::atomic<std::size_t> totalTaskCount;
    thread_local std::size_t taskCount;
    std::vector<bcpp::system::work_contract> workContracts(max_contracts);
    for (auto i = 0; i < max_contracts; ++i)
        workContracts[i] = workContractGroup.create_contract([&, index = i]() mutable{++taskCount; workContracts[index = contractId[index]].invoke();});

    // invoke the correct number of concurrent contracts to start things off
    for (auto i = 0; i < max_concurrent_contracts; ++i)
        workContracts[i].invoke();

    // create a worker thread pool and direct the threads to service the work contract group - also very simple
    std::vector<bcpp::system::thread_pool::thread_configuration> threads(num_worker_threads);
    auto index = 0;
    for (auto & thread : threads)
    {
        thread.cpuId_ = index++;
        thread.function_ = [&]
                (
                    auto const & stopToken
                ) mutable
                {
                    while (!stopToken.stop_requested()) 
                        workContractGroup.service_contracts(); 
                    totalTaskCount += taskCount;
                    taskCount = 0;
                };
    }
    bcpp::system::thread_pool threadPool({.threads_ = threads});

    auto startTime = std::chrono::system_clock::now();
    std::this_thread::sleep_for(test_duration);
    threadPool.stop(bcpp::system::synchronization_mode::blocking);
    auto stopTime = std::chrono::system_clock::now();
    auto elapsedTime = (stopTime - startTime);
    auto test_duration_in_sec = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(elapsedTime).count() / std::nano::den;

    std::cout << "Total tasks = " << totalTaskCount << ", tasks per sec = " << (int)(totalTaskCount / test_duration_in_sec) << 
            ", tasks per thread per sec = " << (int)((totalTaskCount / test_duration_in_sec) / num_worker_threads) << std::endl;
}


//=============================================================================
int main
(
    int, 
    char const **
)
{    
    bare_minimum_example();
    basic_example();

    static auto constexpr num_loops = 10;
    for (auto i = 0; i < num_loops; ++i)
        multithreaded_example();

    return 0;
}
