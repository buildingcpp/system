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
void priority_example
(
)
{
    static auto constexpr max_priority = 8;
    work_contract_group workContractGroup(16384, max_priority);

    static auto constexpr num_priority_0 = 1024;
    static auto constexpr num_priority_1 = 1;
    static auto constexpr num_priority_2 = 1;
    static auto constexpr num_priority_3 = 1;
    static auto constexpr num_priority_4 = 1;

    std::atomic<std::size_t> counter[5];
    std::size_t numWorkContractsAtPriority[5] = {num_priority_0, num_priority_1, num_priority_2, num_priority_3, num_priority_4};

    auto numWorkContracts = 0;
    for (auto n : numWorkContractsAtPriority)
        numWorkContracts += n;
    std::vector<work_contract> workContracts;

    auto index = 0;
    for (auto priority = 0; priority < 5; ++priority)
    {
        for (auto i = 0; i < numWorkContractsAtPriority[priority]; ++i)
        {
            workContracts.push_back(workContractGroup.create_contract([&, index](){counter[0]++; workContracts[index].invoke();}, [](){}));//, priority));
            ++index;
        }
    }

    for (auto & workContract : workContracts)
        workContract.invoke();

    std::jthread thread([&](std::stop_token const & stopToken){while (!stopToken.stop_requested()) workContractGroup.execute_next_contract();});

    std::this_thread::sleep_for(std::chrono::seconds(1));
    thread.request_stop();
    thread.join();

    for (auto i = 0; i < 5; ++i)
        std::cout << "wc " << i << " = " << counter[i] << std::endl;
}


//=============================================================================
int main
(
    int, 
    char const **
)
{    
    priority_example();
    return 0;
}
