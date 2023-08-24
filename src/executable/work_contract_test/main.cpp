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
auto work_contract_test
(
    // measure performance where max number of contracts is large and where
    // there are always some preconfigured number of contracts invoked.
    std::size_t num_worker_threads
)
{
    // test_duration: how long to run test
    static auto constexpr test_duration = std::chrono::milliseconds(10000);
    // work_contract_capacity: total available work contracts
    static auto constexpr work_contract_capacity = (1 << 10);
    // num_contracts_to_use: number of contracts to use in test
    static auto constexpr num_contracts_to_use = work_contract_capacity;

    // contruct work contract group
    work_contract_group workContractGroup(work_contract_capacity);
    std::vector<work_contract> workContracts(num_contracts_to_use);

    // containers for gathering stats during test
    std::vector<std::int64_t> totalTaskCount(num_contracts_to_use);
    auto useless = 0;

    // create work contracts
    for (auto i = 0; i < num_contracts_to_use; ++i)
    {
        workContracts[i] = workContractGroup.create_contract(
                [&, index = i]
                (
                    // the work to do.  
                    // each time work contract is executed increase counter and then re-invoke the same contract again.
                )
                {
                    useless += work_function();
                    totalTaskCount[index]++;
                    workContracts[index].invoke();
                });
    }

    // invoke each of the contracts to start things off
    for (auto & workContract : workContracts)
        workContract.invoke();

    // create a worker thread pool and direct the threads to service the work contract group
    // ensure that each thread is on its own cpu for the sake of this test
    std::vector<bcpp::system::thread_pool::thread_configuration> threads(num_worker_threads);
    for (auto && [index, thread] : ranges::v3::views::enumerate(threads))
    {
        thread.cpuId_ = index * 2;
        thread.function_ = [&]
                (
                    auto const & stopToken
                ) mutable
                {
                    while (!stopToken.stop_requested())
                        workContractGroup.execute_next_contract();//std::chrono::seconds(1));
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
    // gather timing
    auto stopTime = std::chrono::system_clock::now();
    auto elapsedTime = (stopTime - startTime);
    auto test_duration_in_sec = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(elapsedTime).count() / std::nano::den;
    // calculate std deviation etc
    std::size_t n = 0;    
    for (auto i = 0; i < num_contracts_to_use; ++i)
        n += totalTaskCount[i];
    long double m = ((long double)n / num_contracts_to_use);
    long double k = 0;
    for (auto i = 0; i < num_contracts_to_use; ++i)
        k += ((totalTaskCount[i] - m) * (totalTaskCount[i] - m));
    k /= (num_contracts_to_use - 1);
    auto sd = std::sqrt(k);
    // report results
    std::cout << "Threads = " << num_worker_threads << " , total tasks = " << n << " , tasks per sec = " << (int)(n / test_duration_in_sec) << 
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
  //  bare_minimum_example();
  //  basic_example();
  //  work_contract_after_group_destroyed_test();

    static auto constexpr num_loops = 10;
    auto n = 0;
    for (auto i = 0; i < num_loops; ++i)
        n += work_contract_test(i + 1);

    return n;
}
