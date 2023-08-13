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


using namespace bcpp::system;


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


//=============================================================================
void measure_multithreaded_concurrent_contracts
(
    // measure performance where max number of contracts is large and where
    // there are always some preconfigured number of contracts invoked.
)
{
    static auto const num_worker_threads = std::thread::hardware_concurrency() / 2;
    static auto constexpr test_duration = std::chrono::milliseconds(1000);
    static auto constexpr max_contracts = (1 << 16);
    static auto const max_concurrent_contracts = num_worker_threads / 2;
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
    work_contract_group workContractGroup(max_contracts);
    std::atomic<std::size_t> totalTaskCount[max_contracts];
    thread_local std::size_t taskCount;
    std::vector<work_contract> workContracts(max_contracts);

    for (auto i = 0; i < max_contracts; ++i)
        workContracts[i] = workContractGroup.create_contract([&, index = i]() mutable
                {
                //    std::this_thread::sleep_for(std::chrono::microseconds(1));
                    ++taskCount;
                    totalTaskCount[index]++;
                    workContracts[index].invoke();
                    return;

                    if (index & 1)
                    {
                        if (totalTaskCount[index] & 1)
                            workContracts[index].invoke();
                    }
                    else
                    {
                        if ((totalTaskCount[index] & 0x07) == 0x07)
                            workContracts[index + 1].invoke();
                        workContracts[index].invoke();
                    }
                });

    // invoke the correct number of concurrent contracts to start things off
    for (auto i = 0; i < max_contracts; ++i)
        workContracts[i].invoke();

    // create a worker thread pool and direct the threads to service the work contract group - also very simple
    std::vector<bcpp::system::thread_pool::thread_configuration> threads(num_worker_threads);
    for (auto && [index, thread] : ranges::v3::views::enumerate(threads))
    {
        thread.cpuId_ = index;
        thread.function_ = [&]
                (
                    auto const & stopToken
                ) mutable
                {
                    while (!stopToken.stop_requested()) 
                        workContractGroup.execute_next_contract(std::chrono::seconds(1)); 
                };
    }
    bcpp::system::thread_pool threadPool({.threads_ = threads});

    // start test
    auto startTime = std::chrono::system_clock::now();
    // wait for duration of test
    std::this_thread::sleep_for(test_duration);
    // stop worker threads
    threadPool.stop(synchronization_mode::async);
    // wait for all threads to exit
    threadPool.wait_stop_complete();
    workContractGroup.stop();
    // test completed
    auto stopTime = std::chrono::system_clock::now();
    auto elapsedTime = (stopTime - startTime);
    auto test_duration_in_sec = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(elapsedTime).count() / std::nano::den;

    auto n = 0;    
    for (auto i = 0; i < max_contracts; ++i)
    {
    //    std::cout << "contract " << i << " executed " << totalTaskCount[i] << " times\n";
        n += totalTaskCount[i];
    }
    std::cout << "Total tasks = " << n << ", tasks per sec = " << (int)(n / test_duration_in_sec) << 
            ", tasks per thread per sec = " << (int)((n / test_duration_in_sec) / num_worker_threads) << std::endl;


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
    work_contract_after_group_destroyed_test();

    static auto constexpr num_loops = 10;
    for (auto i = 0; i < num_loops; ++i)
        measure_multithreaded_concurrent_contracts();

    return 0;
}
