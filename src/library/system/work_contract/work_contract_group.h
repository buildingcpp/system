#pragma once

#include <range/v3/view/enumerate.hpp>

#include <memory>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <functional>


namespace bcpp::system 
{

    class work_contract;


    class work_contract_group
    {
    public:

        // allow_wait
        //
        // TRUE: uses condition_variable to allow threads to sleep while waiting for contracts to execute.
        // This mode is better for standard applications.
        //
        // FALSE: threads will return immediately if there are no contracts to execute.  
        // This mode is for spinning and is useful for low latency environments.
        
        static auto constexpr allow_wait = true;

        class surrender_token;

        work_contract_group
        (
            std::int64_t
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

        void execute_next_contract();

        void execute_next_contract
        (
            std::chrono::nanoseconds
        );

        std::size_t get_capacity() const;

        std::size_t get_active_contract_count() const;

        void stop();

    private:

        friend class work_contract;
        friend class surrender_token;

        static auto constexpr left_addend = 0x0000000000000001ull;
        static auto constexpr left_mask = 0x00000000ffffffffull;
        static auto constexpr right_mask = 0xffffffff00000000ull;
        static auto constexpr right_addend = 0x0000000100000000ull;

        struct contract
        {
            static auto constexpr surrender_flag    = 0x00000004;
            static auto constexpr execute_flag      = 0x00000002;
            static auto constexpr invoke_flag       = 0x00000001;
        
            std::function<void()>       work_;
            std::function<void()>       surrender_;
            std::atomic<std::int32_t>   flags_;
        };

        void invoke
        (
            work_contract const &
        );

        void surrender
        (
            work_contract const &
        );        
        
        template <std::size_t>
        void set_contract_flag
        (
            work_contract const &
        );

        enum class inclination : std::uint32_t
        {
            left = 0,
            right = 1
        };

        template <inclination>
        std::pair<std::int64_t, std::uint64_t> decrement_contract_count(std::int64_t);

        std::size_t process_contract();

        void process_contract(std::int64_t);

        void increment_contract_count(std::int64_t);

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
        std::uint64_t select_contract_bit
        (
            std::uint64_t,
            std::uint64_t,
            std::uint64_t
        ) const;

        std::size_t get_available_contract();

        std::uint64_t const capacity_;
        std::vector<invocation_counter>                 invocationCounter_;
        std::vector<std::atomic<std::uint64_t>>         invokedFlag_;

        std::vector<invocation_counter>                 contractCounter_;

        std::vector<contract>                           contracts_;

        std::vector<std::shared_ptr<surrender_token>>   surrenderToken_;

        std::int64_t                                    firstContractIndex_;

        std::mutex                                      mutex_;

        std::atomic<bool>                               stopped_{false};

        // for 'waitable' version
        std::condition_variable                         conditionVariable_;
        std::atomic<std::size_t>                        activeCount_;
    }; // class work_contract_group


    class work_contract_group::surrender_token
    {
    public:

        surrender_token
        (
            work_contract_group *
        );
        
        std::mutex mutex_;
        work_contract_group * workContractGroup_{};

        bool invoke(work_contract const &);

        void orphan();
    };

} // namespace bcpp::system


#include "./work_contract.h"


//=============================================================================
inline bcpp::system::work_contract_group::work_contract_group
(
    std::int64_t capacity
):
    capacity_((capacity < 256) ? 256 : ((capacity + 63) / 64) * 64),
    invocationCounter_(capacity_ / 32),
    contractCounter_(capacity_),
    invokedFlag_(capacity_ / 64),
    contracts_(capacity_),
    surrenderToken_(capacity_)
{
    firstContractIndex_ = ((capacity_ / 64) - 1);

    std::uint32_t c = capacity_;
    auto n = 1;
    auto k = 0;
    while (c > 1)
    {
        for (auto i = 0; i < n; ++i)
            contractCounter_[k++].u32_ = {c / 2, c / 2};
        n <<= 1;
        c >>= 1;
    }
}


//=============================================================================
inline bcpp::system::work_contract_group::~work_contract_group
(
)
{
    stop();
}


//=============================================================================
inline void bcpp::system::work_contract_group::stop
(
)
{
    if (bool wasRunning = !stopped_.exchange(true); wasRunning)
    {
        for (auto & surrenderToken : surrenderToken_)
            if ((bool)surrenderToken)
                surrenderToken->orphan();
        if constexpr (allow_wait)
        {
            std::unique_lock uniqueLock(mutex_);
            conditionVariable_.notify_all();
        }
    }
}


//=============================================================================
inline auto bcpp::system::work_contract_group::create_contract
(
    std::function<void()> function
) -> work_contract
{
    return create_contract(function, nullptr);
}


//=============================================================================
inline auto bcpp::system::work_contract_group::create_contract
(
    std::function<void()> function,
    std::function<void()> surrender
) -> work_contract
{

    std::uint32_t contractId = get_available_contract();
    if (contractId == ~0)
        return {}; // no free contracts
    auto & contract = contracts_[contractId];
    contract.flags_ = 0;
    contract.work_ = function;
    contract.surrender_ = surrender;
    auto surrenderToken = surrenderToken_[contractId] = std::make_shared<surrender_token>(this);
    return {this, surrenderToken, contractId};
}


//=============================================================================
inline void bcpp::system::work_contract_group::surrender
(
    work_contract const & workContract
)
{
    set_contract_flag<contract::surrender_flag | contract::invoke_flag>(workContract);
}


//=============================================================================
inline void bcpp::system::work_contract_group::invoke
(
    work_contract const & workContract
)
{
    set_contract_flag<contract::invoke_flag>(workContract);
}


//=============================================================================
template <std::size_t flags_to_set>
inline void bcpp::system::work_contract_group::set_contract_flag
(
    work_contract const & workContract
)
{
    static auto constexpr flags_mask = (contract::execute_flag | contract::invoke_flag);
    auto contractId = workContract.get_id();
    if ((contracts_[contractId].flags_.fetch_or(flags_to_set) & flags_mask) == 0)
        increment_contract_count(contractId);
}


//=============================================================================
inline std::size_t bcpp::system::work_contract_group::get_available_contract
(
    // find an unclaimed contract
    // do so with consideration to how balanced the tree is
)
{
    auto select_child = [&]
    (
        std::size_t parent
    )
    {
        auto & contractCounter = contractCounter_[parent].u64_;
        auto expected = contractCounter.load();
        auto addend = ((expected & 0xffffffff) > (expected >> 32)) ? left_addend : right_addend;
        while ((expected != 0) && (!contractCounter.compare_exchange_strong(expected, expected - addend)))
            addend = ((expected & 0xffffffff) > (expected >> 32)) ? left_addend : right_addend;
        return expected ? ((parent * 2) + 1 + (addend == right_addend)) : 0;
    };

    auto parent = select_child(0);
    while ((parent) && (parent < (capacity_ - 1)))
        parent = select_child(parent);
    if (parent)
        return (parent - (capacity_ - 1));
    return ~0;
}


//=============================================================================
inline std::size_t bcpp::system::work_contract_group::get_active_contract_count
(
) const
{
    return invocationCounter_[0].get_count();
}


//=============================================================================
inline void bcpp::system::work_contract_group::increment_contract_count
(
    std::int64_t current
)
{
    invokedFlag_[current / 64] |= ((0x8000000000000000ull) >> (current & 63));
    current /= 64;

    std::uint64_t rootCount = 0;
    current += firstContractIndex_;
    while (current)
    {
        auto addend = ((current-- & 1ull) ? left_addend : right_addend);
        rootCount = (invocationCounter_[current >>= 1].u64_ += addend);
    }

    if constexpr (allow_wait)
    {
        if ((((rootCount >> 32) + rootCount) & 0xffffffff) == 1)
        {
            if (++activeCount_ == 1)
            {
                std::lock_guard lockGuard(mutex_);
                conditionVariable_.notify_all();
            }
        }
    }
}


//=============================================================================
template <bcpp::system::work_contract_group::inclination T_>
inline auto bcpp::system::work_contract_group::decrement_contract_count
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
template <std::uint64_t N>
inline std::uint64_t bcpp::system::work_contract_group::select_contract_bit
(
    std::uint64_t inclinationFlags,
    std::uint64_t invokedFlags,
    std::uint64_t selection
) const
{
    if constexpr (N == 0)
    {
        return selection;
    }
    else
    {
        auto constexpr bits_to_consider = (1 << N);
        std::uint64_t constexpr bit_mask[2]{~((1ull << (bits_to_consider / 2)) - 1), ((1ull << (bits_to_consider / 2)) - 1)};

        auto preferRight = ((inclinationFlags & 1) == 1);
        if (auto usePreferedSide = ((invokedFlags & bit_mask[preferRight]) != 0); usePreferedSide == preferRight)
            selection += (bits_to_consider / 2);
        else
            invokedFlags >>= (bits_to_consider / 2);
        return select_contract_bit<N - 1>(inclinationFlags >> 1, invokedFlags & bit_mask[1], selection);
    }
}


//=============================================================================
inline void bcpp::system::work_contract_group::execute_next_contract
(
    std::chrono::nanoseconds duration
)
{
    if constexpr (allow_wait)
    {
        if (activeCount_ == 0)
        {
            std::unique_lock uniqueLock(mutex_);
            conditionVariable_.wait_for(uniqueLock, duration, [this](){return ((activeCount_ > 0) || (stopped_));});
        }
    }
    execute_next_contract();
}


//=============================================================================
template <std::uint64_t N>
inline std::uint64_t bcpp::system::work_contract_group::select_contract
(
    std::uint64_t & inclinationFlags,
    std::uint64_t parent
)
{
    static auto constexpr logical_max_n = ((sizeof(inclinationFlags) * 8) - 1);
    if constexpr (N >= logical_max_n)
    {
        return 0;
    }
    else
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
}


//=============================================================================
inline void bcpp::system::work_contract_group::execute_next_contract
(
)
{
    static thread_local std::uint64_t tls_inclinationFlags = 0;
    static auto constexpr right = inclination::right;
    static auto constexpr left = inclination::left;
    auto inclinationFlags = tls_inclinationFlags++;
    auto [parent, rootCount] = ((inclinationFlags & 1) ? decrement_contract_count<right>(0) : decrement_contract_count<left>(0));
    if (parent)  
    {
        if constexpr (allow_wait)
        {
            if (rootCount == 0)
                --activeCount_;
        }
        // select parent node in heap
        parent = select_contract<1>(inclinationFlags, parent);
        // select bit which represents the contract to execute
        auto invokedFlagsIndex = (parent - firstContractIndex_);
        auto & invokedFlags = invokedFlag_[invokedFlagsIndex];
        while (true)
        {
            auto expected = invokedFlags.load();
            auto contractIndex = select_contract_bit<6>(inclinationFlags, expected, 0);
            auto bit = (0x8000000000000000ull >> contractIndex);
            expected |= bit;
            if (invokedFlags.compare_exchange_strong(expected, expected & ~bit))
                return process_contract((invokedFlagsIndex * 64) + contractIndex);
        }
    }
}


//=============================================================================
inline void bcpp::system::work_contract_group::process_contract
(
    std::int64_t contractId
)
{
    auto & contract = contracts_[contractId];
    auto & flags = contract.flags_;
    if ((++flags & contract::surrender_flag) != contract::surrender_flag)
    {
        contract.work_();
        if (((flags -= contract::execute_flag) & contract::invoke_flag) == contract::invoke_flag)
            increment_contract_count(contractId);
    }
    else
    {
        if (contract.surrender_)
            std::exchange(contract.surrender_, nullptr)();
        contract.work_ = nullptr;
        surrenderToken_[contractId] = {};
        contractId += (capacity_ - 1);
        while (contractId)
            contractCounter_[contractId >>= 1].u64_ += ((contractId-- & 1ull) ? left_addend : right_addend);
    }
}


//=============================================================================
inline std::size_t bcpp::system::work_contract_group::get_capacity
(
) const
{
    return contracts_.size();
}


//=============================================================================
inline bcpp::system::work_contract_group::surrender_token::surrender_token
(
    work_contract_group * workContractGroup
):
    workContractGroup_(workContractGroup)
{
}


//=============================================================================
inline bool bcpp::system::work_contract_group::surrender_token::invoke
(
    work_contract const & workContract
)
{
    std::lock_guard lockGuard(mutex_);
    if (auto workContractGroup = std::exchange(workContractGroup_, nullptr); workContractGroup != nullptr)
    {
        workContractGroup->surrender(workContract);
        return true;
    }
    return false;
}


//=============================================================================
inline void bcpp::system::work_contract_group::surrender_token::orphan
(
)
{
    std::lock_guard lockGuard(mutex_);
    workContractGroup_ = nullptr;
}
