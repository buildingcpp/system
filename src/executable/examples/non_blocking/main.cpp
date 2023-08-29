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
    auto workContract = std::move(workContractGroup.create_contract([&](){std::cout << "executed\n"; done = true;}));
    // create async thread to process the work contract
    std::jthread workerThread([&](){while (!done) workContractGroup.execute_next_contract();});
    // delay for one second before scheduling the work contract
    std::this_thread::sleep_for(std::chrono::seconds(1));
    // schedule and it should be executed very soon by waiting worker thread
    workContract.schedule();
    
    while (!done)
        ;

    return 0;
}
