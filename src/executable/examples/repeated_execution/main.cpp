#include <cstddef>
#include <iostream>
#include <atomic>
#include <chrono>

#include <library/system.h>


//=============================================================================
int main()
{
    std::atomic<bool> done{false};

    // create a non blocking work contract group
    bcpp::system::work_contract_group workContractGroup(256);
    // create a contract
    bcpp::system::work_contract workContract = std::move(workContractGroup.create_contract(
            [&, counter = 0]
            (
            ) mutable
            {
                static auto constexpr max_counter = 5;
                std::cout << "executed " << ++counter << " times\n"; 
                if (counter >= max_counter) 
                    workContract.release(); // on last round, release 
                else
                    workContract.schedule(); // otherwise, once executed, schedule for another execution
            }));
    workContract.schedule();
    
    // create async thread to process the work contract
    std::jthread workerThread([&](){while (workContract) workContractGroup.execute_next_contract();});

    while (workContract)
        ; // stick arount until the work contract is released

    return 0;
}
