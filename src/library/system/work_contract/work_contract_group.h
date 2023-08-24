#pragma once

#include "./basic_work_contract_group.h"

#include <include/non_movable.h>
#include <include/non_copyable.h>

#include <memory>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>


namespace bcpp::system
{

    class work_contract_group :
        non_movable,
        non_copyable
    {
    public:

        static auto constexpr fold = 16;
        static auto constexpr fold_mask = fold - 1;

        work_contract_group
        (
            std::uint64_t
        );

        ~work_contract_group();

        work_contract create_contract
        (
            std::function<void()>
        );

        work_contract create_contract
        (
            std::function<void()>,
            std::function<void()>
        );

        void execute_next_contract
        (
            std::chrono::nanoseconds
        );

        void execute_next_contract();

        void stop();

    private:

        std::atomic<std::uint64_t>                  counter_;
        std::shared_ptr<waitable_state>             waitableState_;
        std::array<basic_work_contract_group, fold> workContractGroup_;


        template <std::size_t ... N>
        decltype(workContractGroup_) initialize_work_contract_group_array
        (
            std::index_sequence<N ...>,
            std::uint64_t capacity
        ) 
        {
            return {basic_work_contract_group(capacity + N - N, waitableState_) ...};
        }

    };

} // namespace bcpp::system


//=============================================================================
inline bcpp::system::work_contract_group::work_contract_group
(
    std::uint64_t capacity
):
    waitableState_(std::make_shared<waitable_state>()),
    workContractGroup_(initialize_work_contract_group_array(std::make_index_sequence<fold>(), capacity / fold))
{
}


//=============================================================================
inline bcpp::system::work_contract_group::~work_contract_group()
{
    stop();
}


//=============================================================================
inline auto bcpp::system::work_contract_group::create_contract
(
    std::function<void()> f
) -> work_contract
{
    return create_contract(f, nullptr);
}


//=============================================================================
inline auto bcpp::system::work_contract_group::create_contract
(
    std::function<void()> f,
    std::function<void()> s
) -> work_contract
{
    for (auto i = 0; i < fold; ++i)
        if (auto workContract = workContractGroup_[counter_++ & fold_mask].create_contract(f, s); workContract) 
            return workContract;
    return {};
}


//=============================================================================
inline void bcpp::system::work_contract_group::execute_next_contract
(
    std::chrono::nanoseconds duration
)
{
    if constexpr (allow_blocking)
    {
        if (waitableState_->wait_for(duration))
            execute_next_contract();
    }
    else
    {
        execute_next_contract();
    }
}


//=============================================================================
inline void bcpp::system::work_contract_group::execute_next_contract
(
)
{
    static thread_local std::size_t counter = 0;
    static thread_local std::uint64_t tls_inclinationFlags[fold];
    for (auto i = 0; i < fold; ++i, ++counter)
        if (auto index = (counter++ & fold_mask); workContractGroup_[index].execute_next_contract(tls_inclinationFlags[index]++)) 
            return;
}


//=============================================================================
inline void bcpp::system::work_contract_group::stop
(
)
{
    for (auto & workContractGroup : workContractGroup_)
        workContractGroup.stop();
    waitableState_->stop();
}
