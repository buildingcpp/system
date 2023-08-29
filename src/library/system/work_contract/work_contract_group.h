#pragma once

#include "./work_contract_mode.h"
#include "./sub_work_contract_group.h"

#include <include/non_movable.h>
#include <include/non_copyable.h>

#include <memory>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <concepts>


namespace bcpp::system
{

    namespace internal
    {
        template <work_contract_mode T>
        class work_contract_group :
            non_movable,
            non_copyable
        {
        public:

            static auto constexpr mode = T;

            using work_contract_type = work_contract<mode>;
            using sub_work_contract_group_type = sub_work_contract_group<mode>;

            static auto constexpr fold = 256;
            static auto constexpr fold_mask = fold - 1;

            work_contract_group
            (
                std::uint64_t
            );

            ~work_contract_group();

            work_contract_type create_contract
            (
                std::invocable auto &&
            );

            work_contract_type create_contract
            (
                std::invocable auto &&,
                std::invocable auto &&
            );

            void execute_next_contract
            (
                std::chrono::nanoseconds
            ) requires (mode == work_contract_mode::blocking);

            void execute_next_contract();

            void stop();

        private:

            std::atomic<std::uint64_t>                  counter_;
            std::shared_ptr<waitable_state>             waitableState_;
            std::array<sub_work_contract_group_type, fold> workContractGroup_;


            template <std::size_t ... N>
            decltype(workContractGroup_) initialize_work_contract_group_array
            (
                std::index_sequence<N ...>,
                std::uint64_t capacity
            ) requires (mode == work_contract_mode::blocking)
            {
                return {sub_work_contract_group_type(capacity + N - N, waitableState_) ...};
            }

            template <std::size_t ... N>
            decltype(workContractGroup_) initialize_work_contract_group_array
            (
                std::index_sequence<N ...>,
                std::uint64_t capacity
            ) requires (mode == work_contract_mode::non_blocking)
            {
                return {sub_work_contract_group_type(capacity + N - N) ...};
            }

        };

    } // namespace internal

    using work_contract_group = internal::work_contract_group<internal::work_contract_mode::non_blocking>;
    using blocking_work_contract_group = internal::work_contract_group<internal::work_contract_mode::blocking>;


} // namespace bcpp::system


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline bcpp::system::internal::work_contract_group<T>::work_contract_group
(
    std::uint64_t capacity
):
    waitableState_(std::make_shared<waitable_state>()),
    workContractGroup_(initialize_work_contract_group_array(std::make_index_sequence<fold>(), capacity / fold))
{
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline bcpp::system::internal::work_contract_group<T>::~work_contract_group
(
)
{
    stop();
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline auto bcpp::system::internal::work_contract_group<T>::create_contract
(
    std::invocable auto && workFunction
) -> work_contract_type
{
    return create_contract(std::forward<decltype(workFunction)>(workFunction), +[](){});
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline auto bcpp::system::internal::work_contract_group<T>::create_contract
(
    std::invocable auto && workFunction,
    std::invocable auto && releaseFunction
) -> work_contract_type
{
    for (auto i = 0; i < fold; ++i)
        if (auto workContract = workContractGroup_[counter_++ & fold_mask].create_contract(std::forward<decltype(workFunction)>(workFunction), std::forward<decltype(releaseFunction)>(releaseFunction)); workContract) 
            return workContract;
    return {};
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline void bcpp::system::internal::work_contract_group<T>::execute_next_contract
(
    std::chrono::nanoseconds duration
) requires (mode == work_contract_mode::blocking)
{
    if (waitableState_->wait_for(duration))
        execute_next_contract();
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline void bcpp::system::internal::work_contract_group<T>::execute_next_contract
(
)
{
    static thread_local std::size_t counter = 0;
    static std::atomic<std::size_t> stride_ = 1;
    static thread_local std::size_t stride = 1;// TODO ((stride_ += 2) % fold_mask);
    static thread_local std::uint64_t tls_inclinationFlags[fold];
    for (auto i = 0; i < fold; ++i)
        if (auto index = ((counter += stride) & fold_mask); workContractGroup_[index].execute_next_contract(tls_inclinationFlags[index]++)) 
            return;
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline void bcpp::system::internal::work_contract_group<T>::stop
(
)
{
    for (auto & workContractGroup : workContractGroup_)
        workContractGroup.stop();
    waitableState_->stop();
}
