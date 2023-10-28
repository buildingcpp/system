#include <cstddef>
#include <iostream>
#include <chrono>
#include <functional>
#include <thread>
#include <mutex>
#include <map>
#include <atomic>

#include "./widget.h"

#include <include/non_copyable.h>
#include <include/non_movable.h>
#include <library/system.h>


//=============================================================================
int main()
{
    static auto constexpr num_worker_threads = 5;
    bcpp::system::blocking_work_contract_group workContractGroup(256);
    widget_registry widgetRegistry;

    std::vector<std::jthread> workerThreads(num_worker_threads);
    for (auto & workerThread : workerThreads)
        workerThread = std::move(std::jthread([&](auto const & stopToken)
                {while (!stopToken.stop_requested()) workContractGroup.execute_next_contract(std::chrono::milliseconds(10));}));

    std::jthread widgeThread([&](std::stop_token stopToken){while (!stopToken.stop_requested()) widgetRegistry.widge_all();});

    {
        widget w(&widgetRegistry, workContractGroup);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    for (auto & workerThread : workerThreads)
    {
        workerThread.request_stop();
        workerThread.join();
    }
    
    widgeThread.request_stop();

    return 0;
}
