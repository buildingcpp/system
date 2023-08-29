#pragma once

#include "./work_contract_mode.h"
#include "./waitable_state.h"

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
        template <work_contract_mode>
        class work_contract;


        template <work_contract_mode T>
        class sub_work_contract_group :
            non_copyable,
            non_movable
        {
        public:

            static auto constexpr mode = T;
            using work_contract_type = work_contract<mode>;

            class release_token;

            sub_work_contract_group
            (
                std::uint64_t
            ) requires (mode == work_contract_mode::non_blocking);

            sub_work_contract_group
            (
                std::uint64_t,
                std::shared_ptr<waitable_state>
            ) requires (mode == work_contract_mode::blocking);

            ~sub_work_contract_group();

            work_contract_type create_contract
            (
                std::invocable auto &&
            );

            work_contract_type create_contract
            (
                std::invocable auto &&,
                std::invocable auto &&
            );

            bool execute_next_contract
            (
                std::uint64_t
            );

            void stop();

        private:

            friend class work_contract<mode>;
            friend class release_token;

            static auto constexpr left_addend   = 0x0000000000000001ull;
            static auto constexpr left_mask     = 0x00000000ffffffffull;
            static auto constexpr right_mask    = 0xffffffff00000000ull;
            static auto constexpr right_addend  = 0x0000000100000000ull;

            struct contract
            {
                static auto constexpr release_flag  = 0x00000004;
                static auto constexpr execute_flag  = 0x00000002;
                static auto constexpr schedule_flag = 0x00000001;
            
                std::function<void()>       work_;
                std::function<void()>       release_;
                std::atomic<std::int32_t>   flags_;
            };

            void schedule
            (
                work_contract_type const &
            );

            void release
            (
                work_contract_type const &
            );        
            
            template <std::size_t>
            void set_contract_flag
            (
                work_contract_type const &
            );

            enum class inclination : std::uint32_t
            {
                left = 0,
                right = 1
            };

            template <inclination>
            std::pair<std::int64_t, std::uint64_t> decrement_contract_count(std::int64_t);

            std::size_t process_contract();

            void process_release(std::int64_t);

            void process_contract(std::int64_t);

            void increment_contract_count
            (
                std::int64_t
            ) requires (mode == work_contract_mode::blocking);
            
            void increment_contract_count
            (
                std::int64_t
            ) requires (mode == work_contract_mode::non_blocking);

            union alignas(8) invocation_counter
            {
                invocation_counter():u64_(){static_assert(sizeof(*this) == sizeof(std::uint64_t));}
                std::atomic<std::uint64_t> u64_;
                struct parts
                {
                    std::uint32_t left_;
                    std::uint32_t right_;
                } u32_;
                std::uint64_t get_count() const
                {
                    auto n = u64_.load();
                    return ((n >> 32) + (n & 0xffffffff));
                }
            };

            template <std::uint64_t N>
            std::uint64_t select_contract
            (
                std::uint64_t &,
                std::uint64_t
            );

            template <std::uint64_t N>
            std::uint64_t select_scheduled_contract_bit
            (
                std::uint64_t,
                std::uint64_t,
                std::uint64_t
            ) const;

            template <std::uint64_t N>
            std::uint64_t get_available_contract_bit
            (
                std::uint64_t,
                std::uint64_t
            );

            std::size_t get_available_contract();

            std::uint64_t                                   capacity_;
            std::vector<invocation_counter>                 invocationCounter_;
            std::vector<std::atomic<std::uint64_t>>         scheduledFlag_;

            std::vector<invocation_counter>                 availableCounter_;
            std::vector<std::atomic<std::uint64_t>>         availableFlag_;

            std::vector<contract>                           contracts_;

            std::vector<std::shared_ptr<release_token>>     releaseToken_;

            std::int64_t                                    firstContractIndex_;

            std::mutex                                      mutex_;

            std::shared_ptr<waitable_state>                 waitableState_;

            std::atomic<bool>                               stopped_{false};

        }; // class sub_work_contract_group


        template <work_contract_mode T>
        class sub_work_contract_group<T>::release_token
        {
        public:

            release_token
            (
                sub_work_contract_group *
            );
            
            std::mutex mutex_;
            sub_work_contract_group * workContractGroup_{};

            bool schedule(work_contract_type const &);

            void orphan();
        };

    } // namespace internal

} // namespace bcpp::system


#include "./work_contract.h"


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline bcpp::system::internal::sub_work_contract_group<T>::sub_work_contract_group
(
    std::uint64_t capacity,
    std::shared_ptr<waitable_state> waitableState
) requires (mode == work_contract_mode::blocking) :
    capacity_((capacity < 256) ? 256 : ((capacity + 63) / 64) * 64),
    invocationCounter_(capacity_ / 64),
    scheduledFlag_(capacity_ / 64),
    availableCounter_(capacity_ / 64),
    availableFlag_(capacity_ / 64),
    contracts_(capacity_),
    releaseToken_(capacity_),
    waitableState_(waitableState),
    firstContractIndex_((capacity_ / 64) - 1)
{
    std::uint32_t c = capacity_;
    auto n = 1;
    auto k = 0;
    while (c > 64)
    {
        for (auto i = 0; i < n; ++i)
            availableCounter_[k++].u32_ = {c / 2, c / 2};
        n <<= 1;
        c >>= 1;
    }
    for (auto & _ : availableFlag_)
        _ = 0xffffffffffffffffull;
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline bcpp::system::internal::sub_work_contract_group<T>::sub_work_contract_group
(
    std::uint64_t capacity
) requires (mode == work_contract_mode::non_blocking) :
    capacity_((capacity < 256) ? 256 : ((capacity + 63) / 64) * 64),
    invocationCounter_(capacity_ / 64),
    scheduledFlag_(capacity_ / 64),
    availableCounter_(capacity_ / 64),
    availableFlag_(capacity_ / 64),
    contracts_(capacity_),
    releaseToken_(capacity_),
    firstContractIndex_((capacity_ / 64) - 1)
{
    std::uint32_t c = capacity_;
    auto n = 1;
    auto k = 0;
    while (c > 64)
    {
        for (auto i = 0; i < n; ++i)
            availableCounter_[k++].u32_ = {c / 2, c / 2};
        n <<= 1;
        c >>= 1;
    }
    for (auto & _ : availableFlag_)
        _ = 0xffffffffffffffffull;
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline bcpp::system::internal::sub_work_contract_group<T>::~sub_work_contract_group
(
)
{
    stop();
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline void bcpp::system::internal::sub_work_contract_group<T>::stop
(
)
{
    if (bool wasRunning = !stopped_.exchange(true); wasRunning)
    {
        for (auto & releaseToken : releaseToken_)
            if ((bool)releaseToken)
                releaseToken->orphan();
    }
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline auto bcpp::system::internal::sub_work_contract_group<T>::create_contract
(
    std::invocable auto && workFunction
) -> work_contract_type
{
    return create_contract(std::forward<decltype(workFunction)>(workFunction), +[](){});
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline auto bcpp::system::internal::sub_work_contract_group<T>::create_contract
(
    std::invocable auto && workFunction,
    std::invocable auto && releaseFunction
) -> work_contract_type
{
    std::uint32_t contractId = get_available_contract();
    if (contractId == ~0)
        return {}; // no free contracts
    auto & contract = contracts_[contractId];
    contract.flags_ = 0;
    contract.work_ = std::forward<decltype(workFunction)>(workFunction);
    contract.release_ = std::forward<decltype(releaseFunction)>(releaseFunction);
    auto releaseToken = releaseToken_[contractId] = std::make_shared<release_token>(this);
    return {this, releaseToken, contractId};
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline void bcpp::system::internal::sub_work_contract_group<T>::release
(
    work_contract_type const & workContract
)
{
    set_contract_flag<contract::release_flag | contract::schedule_flag>(workContract);
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline void bcpp::system::internal::sub_work_contract_group<T>::schedule
(
    work_contract_type const & workContract
)
{
    set_contract_flag<contract::schedule_flag>(workContract);
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
template <std::size_t flags_to_set>
inline void bcpp::system::internal::sub_work_contract_group<T>::set_contract_flag
(
    work_contract_type const & workContract
)
{
    static auto constexpr flags_mask = (contract::execute_flag | contract::schedule_flag);
    auto contractId = workContract.get_id();
    if ((contracts_[contractId].flags_.fetch_or(flags_to_set) & flags_mask) == 0)
        increment_contract_count(contractId);
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
template <std::uint64_t N>
inline std::uint64_t bcpp::system::internal::sub_work_contract_group<T>::get_available_contract_bit
(
    std::uint64_t scheduledFlags,
    std::uint64_t selection
)
{
    if constexpr (N == 0)
    {
        return selection;
    }
    else
    {
        auto constexpr bits_to_consider = (1 << N) / 2;
        static auto constexpr left_bit_mask = ~((1ull << bits_to_consider) - 1);
        static auto constexpr right_bit_mask = ~left_bit_mask;

        auto leftCount = std::popcount(scheduledFlags & left_bit_mask);
        auto rightCount = std::popcount(scheduledFlags & right_bit_mask);
        if (leftCount < rightCount)
            return get_available_contract_bit<N - 1>(scheduledFlags & right_bit_mask, selection + bits_to_consider);
        return get_available_contract_bit<N - 1>((scheduledFlags >> bits_to_consider) & right_bit_mask, selection);
    }
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline std::size_t bcpp::system::internal::sub_work_contract_group<T>::get_available_contract
(
    // find an unclaimed contract
    // do so with consideration to how balanced the tree is
)
{
    auto select_larger_child = [&]
    (
        std::size_t parent
    )
    {
        auto & contractCounter = availableCounter_[parent].u64_;
        auto expected = contractCounter.load();
        auto addend = ((expected & 0xffffffff) > (expected >> 32)) ? left_addend : right_addend;
        while ((expected != 0) && (!contractCounter.compare_exchange_strong(expected, expected - addend)))
            addend = ((expected & 0xffffffff) > (expected >> 32)) ? left_addend : right_addend;
        return expected ? ((parent * 2) + 1 + (addend == right_addend)) : 0;
    };

    auto parent = select_larger_child(0);
    while ((parent) && (parent < firstContractIndex_) && (parent < (capacity_ - 1)))
        parent = select_larger_child(parent);
    if (parent == 0)
        return ~0;

    auto availableFlagIndex = (parent - firstContractIndex_);
    auto & availableFlags = availableFlag_[availableFlagIndex];
    while (true)
    {
        auto expected = availableFlags.load();
        auto contractIndex = get_available_contract_bit<6>(expected, 0);
        auto bit = (0x8000000000000000ull >> contractIndex);
        if (availableFlags.compare_exchange_strong(expected, expected & ~bit))
            return ((availableFlagIndex * 64) + contractIndex);
    }
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline void bcpp::system::internal::sub_work_contract_group<T>::increment_contract_count
(
    std::int64_t current
) requires (mode == work_contract_mode::blocking)
{
    scheduledFlag_[current >> 6] |= ((0x8000000000000000ull) >> (current & 0x3f));
    current >>= 6;
    current += firstContractIndex_;
    std::uint64_t rootCount = 0;
    while (current)
        rootCount = (invocationCounter_[current >>= 1].u64_ += ((current-- & 1ull) ? left_addend : right_addend));
    if ((((rootCount >> 32) + rootCount) & 0xffffffff) == 1)
        waitableState_->increment_activity_count();
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline void bcpp::system::internal::sub_work_contract_group<T>::increment_contract_count
(
    std::int64_t current
) requires (mode == work_contract_mode::non_blocking)
{
    scheduledFlag_[current >> 6] |= ((0x8000000000000000ull) >> (current & 0x3f));
    current >>= 6;
    current += firstContractIndex_;
    while (current)
        invocationCounter_[current >>= 1].u64_ += ((current-- & 1ull) ? left_addend : right_addend);
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
template <std::uint64_t N>
inline std::uint64_t bcpp::system::internal::sub_work_contract_group<T>::select_scheduled_contract_bit
(
    std::uint64_t inclinationFlags,
    std::uint64_t scheduledFlags,
    std::uint64_t selection
) const
{
    if constexpr (N == 0)
    {
        return selection;
    }
    else
    {
        auto constexpr bits_to_consider = (1 << N) / 2;
        std::uint64_t constexpr bit_mask[2]{~((1ull << bits_to_consider) - 1), ((1ull << bits_to_consider) - 1)};

        auto preferRight = ((inclinationFlags & 1) == 1);
        if (auto usePreferedSide = ((scheduledFlags & bit_mask[preferRight]) != 0); usePreferedSide == preferRight)
            selection += bits_to_consider;
        else
            scheduledFlags >>= bits_to_consider;
        return select_scheduled_contract_bit<N - 1>(inclinationFlags >> 1, scheduledFlags & bit_mask[1], selection);
    }
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
template <bcpp::system::internal::sub_work_contract_group<T>::inclination T_>
inline auto bcpp::system::internal::sub_work_contract_group<T>::decrement_contract_count
(
    std::int64_t parent
) -> std::pair<std::int64_t, std::uint64_t> 
{
    static auto constexpr left_inclination = (T_ == inclination::left);
    static auto constexpr mask = (left_inclination) ? left_mask : right_mask;
    static auto constexpr prefered_addend = (left_inclination) ? left_addend : right_addend;
    static auto constexpr fallback_addend = (left_inclination) ? right_addend : left_addend;

    auto & invocationCounter = invocationCounter_[parent].u64_;
    auto expected = invocationCounter.load();
    auto addend = (expected & mask) ? prefered_addend : fallback_addend;
    while ((expected != 0) && (!invocationCounter.compare_exchange_strong(expected, expected - addend)))
        addend = (expected & mask) ? prefered_addend : fallback_addend;
    return {expected ? (1 + (addend > left_mask)) : 0, expected - addend};
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
template <std::uint64_t N>
inline std::uint64_t bcpp::system::internal::sub_work_contract_group<T>::select_contract
(
    std::uint64_t & inclinationFlags,
    std::uint64_t parent
)
{
    static auto constexpr logical_max_n = ((sizeof(inclinationFlags) * 8) - 1);
    if constexpr (N < logical_max_n)
    {
        static auto constexpr bit = (1ull << N);
        static auto constexpr right = inclination::right;
        static auto constexpr left = inclination::left;

        if (parent >= firstContractIndex_)
        {
            inclinationFlags >>= (N + 1);
            return parent;
        }
        auto [n, _] = ((inclinationFlags & bit) ? decrement_contract_count<right>(parent) : decrement_contract_count<left>(parent));
        return select_contract<N + 1>(inclinationFlags, (parent * 2) + n);
    }
    return 0;
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline bool bcpp::system::internal::sub_work_contract_group<T>::execute_next_contract
(
    std::uint64_t inclinationFlags
) 
{
    static auto constexpr right = inclination::right;
    static auto constexpr left = inclination::left;

    auto [parent, rootCount] = ((inclinationFlags & 1) ? decrement_contract_count<right>(0) : decrement_contract_count<left>(0));
    if (parent)  
    {
        if constexpr (mode == work_contract_mode::blocking)
        {
            if (rootCount == 0)
                waitableState_->decrement_activity_count();
        }
        // select parent node in heap
        parent = (parent < firstContractIndex_) ? select_contract<1>(inclinationFlags, parent) : parent;
        // select bit which represents the contract to execute
        auto scheduledFlagsIndex = (parent - firstContractIndex_);
        auto & scheduledFlags = scheduledFlag_[scheduledFlagsIndex];
        while (true)
        {
            auto expected = scheduledFlags.load();
            auto contractIndex = select_scheduled_contract_bit<6>(inclinationFlags, expected, 0);
            auto bit = (0x8000000000000000ull >> contractIndex);
            expected |= bit;
            if (scheduledFlags.compare_exchange_strong(expected, expected & ~bit))
            {
                process_contract((scheduledFlagsIndex * 64) + contractIndex);
                return true;
            }
        }
    }
    return false;
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
inline void bcpp::system::internal::sub_work_contract_group<T>::process_contract
(
    std::int64_t contractId
)
{
    auto & contract = contracts_[contractId];
    auto & flags = contract.flags_;
    if ((++flags & contract::release_flag) != contract::release_flag)
    {
        contract.work_();
        if (((flags -= contract::execute_flag) & contract::schedule_flag) == contract::schedule_flag)
            increment_contract_count(contractId);
    }
    else
    {
        process_release(contractId);
    }
}
