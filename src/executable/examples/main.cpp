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
int main
(
    int, 
    char const **
)
{   
    bare_minimum_example();
    basic_example();
    work_contract_after_group_destroyed_test();

    return 0;
}
