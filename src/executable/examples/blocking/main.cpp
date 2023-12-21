#include <cstddef>
#include <iostream>
#include <atomic>
#include <chrono>

#include <library/system.h>


//=============================================================================
int main()
{
    std::atomic<bool> executed{false};
    std::atomic<bool> released{false};

    // create a blocking work contract group
    bcpp::system::blocking_work_contract_group workContractGroup(256);

    // create a contract
    auto workContract = workContractGroup.create_contract
            (
                [&](){std::cout << "executed\n"; executed = true;}, // this is the 'execution' function (the work function)
                [&](){std::cout << "released\n";  released = true;} // this is the *optional* 'release' function (like a destructor)
            );

    // create async thread to process the work contract within the next 3 seconds
    std::jthread workerThread([&](std::stop_token stopToken)
            {
                while (!stopToken.stop_requested()) 
                    workContractGroup.execute_next_contract(std::chrono::seconds(3));
            });

    // delay for one second before scheduling the work contract
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // schedule and it should be executed very soon by waiting worker thread
    workContract.schedule();

    // wait for the work contract to be executed
    while (!executed)
        ;

    // 'release' the work contract - which will automatically schedule it one more time for the async release function
    workContract.release();

    // wait for the work contract async release function to complete
    while (!released)
        ;
        
    // stop the worker thread
    workerThread.request_stop();
    workerThread.join();
    
    return 0;
}
