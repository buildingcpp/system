#pragma once

#include <range/v3/view/enumerate.hpp>

#include <memory>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <condition_variable>


namespace bcpp::system 
{

    enum class work_contract_mode : std::uint32_t
    {
        non_waitable = 0,
        waitable = 1
    };


    template <work_contract_mode> 
    class work_contract;


    template <work_contract_mode T = work_contract_mode::waitable>
    class work_contract_group
    {
    public:

        static auto constexpr mode = T;
        static auto constexpr waitable = (mode == work_contract_mode::waitable);

        using work_contract_type = work_contract<mode>;

        class surrender_token;

        work_contract_group
        (
            std::int64_t
        );

        ~work_contract_group();

        work_contract_type create_contract
        (
            std::function<void()>
        );

        work_contract_type create_contract
        (
            std::function<void()>,
            std::function<void()>
        );

        std::size_t execute_next_contract();

        std::size_t execute_next_contract
        (
            std::chrono::nanoseconds
        ) requires (waitable);

        std::size_t get_capacity() const;

        std::size_t get_active_contract_count() const;

        void stop();

    private:

        friend class work_contract<T>;
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
            work_contract_type const &
        );

        void surrender
        (
            work_contract_type const &
        );        
        
        template <std::size_t>
        void set_contract_flag
        (
            work_contract_type const &
        );

        enum class decrement_preference : std::uint32_t
        {
            left = 0,
            right = 1
        };

        template <decrement_preference>
        std::int64_t decrement_contract_count(std::int64_t);

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

        std::vector<invocation_counter>                 invocationCounter_;

        std::vector<contract>                           contracts_;

        std::vector<std::shared_ptr<surrender_token>>   surrenderToken_;

        std::int64_t                                    firstContractIndex_;

        std::mutex                                      mutex_;

        std::atomic<std::uint32_t>                      nextAvail_;
        
        std::atomic<std::uint64_t>                      preferenceFlags_;

        std::condition_variable mutable                 conditionVariable_;

        bool                                            stopped_{false};
    }; // class work_contract_group


    template <work_contract_mode T>
    class work_contract_group<T>::surrender_token
    {
    public:

        using work_contract_group_type = work_contract_group<T>;
        using work_contract_type = typename work_contract_group_type::work_contract_type;

        surrender_token
        (
            work_contract_group_type *
        );
        
        std::mutex mutex_;
        work_contract_group_type * workContractGroup_{};

        bool invoke(work_contract_type const &);

        void orphan();
    };


    using waitable_work_contract_group = work_contract_group<work_contract_mode::waitable>;
    using non_waitable_work_contract_group = work_contract_group<work_contract_mode::waitable>;
    using basic_work_contract_group = non_waitable_work_contract_group;

} // namespace bcpp::system


#include "./work_contract.h"


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline bcpp::system::work_contract_group<T>::work_contract_group
(
    std::int64_t capacity
):
    invocationCounter_(capacity - 1), 
    contracts_(capacity),
    surrenderToken_(capacity),
    firstContractIndex_(capacity - 1)
{
    for (auto && [index, contract] : ranges::v3::views::enumerate(contracts_))
        contract.flags_ = (index + 1);
    contracts_.back().flags_ = ~0;
    nextAvail_ = 0;
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline bcpp::system::work_contract_group<T>::~work_contract_group
(
)
{
    stop();
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline void bcpp::system::work_contract_group<T>::stop
(
)
{
    std::lock_guard lockGuard(mutex_);
    stopped_ = true;
    for (auto & surrenderToken : surrenderToken_)
        if ((bool)surrenderToken)
            surrenderToken->orphan();
    conditionVariable_.notify_all();
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline auto bcpp::system::work_contract_group<T>::create_contract
(
    std::function<void()> function
) -> work_contract_type
{
    return create_contract(function, nullptr);
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline auto bcpp::system::work_contract_group<T>::create_contract
(
    std::function<void()> function,
    std::function<void()> surrender
) -> work_contract_type
{
    std::lock_guard lockGuard(mutex_);
    auto contractId = nextAvail_.load();
    if (contractId == ~0)
        return {}; // no free contracts
    auto & contract = contracts_[contractId];
    nextAvail_ = contract.flags_.load();
    contract.flags_ = 0;
    contract.work_ = function;
    contract.surrender_ = surrender;
    auto surrenderToken = surrenderToken_[contractId] = std::make_shared<surrender_token>(this);
    return {this, surrenderToken, contractId};
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline void bcpp::system::work_contract_group<T>::surrender
(
    work_contract_type const & workContract
)
{
    set_contract_flag<contract::surrender_flag | contract::invoke_flag>(workContract);
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline void bcpp::system::work_contract_group<T>::invoke
(
    work_contract_type const & workContract
)
{
    set_contract_flag<contract::invoke_flag>(workContract);
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
template <std::size_t flags_to_set>
inline void bcpp::system::work_contract_group<T>::set_contract_flag
(
    work_contract_type const & workContract
)
{
    static auto constexpr flags_mask = (contract::execute_flag | contract::invoke_flag);
    auto contractId = workContract.get_id();
    if ((contracts_[contractId].flags_.fetch_or(flags_to_set) & flags_mask) == 0)
        increment_contract_count(contractId);
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline std::size_t bcpp::system::work_contract_group<T>::get_active_contract_count
(
) const
{
    return invocationCounter_[0].get_count();
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline void bcpp::system::work_contract_group<T>::increment_contract_count
(
    std::int64_t current
)
{
    current += firstContractIndex_;
    while (current)
    {
        auto addend = ((current-- & 1ull) ? left_addend : right_addend);
        invocationCounter_[current >>= 1].u64_ += addend;
    }
    if constexpr (waitable)
    {
        conditionVariable_.notify_one();
    }
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline std::size_t bcpp::system::work_contract_group<T>::execute_next_contract
(
    std::chrono::nanoseconds maxWaitTime
)  requires (waitable)
{
    if constexpr (waitable)
    {
        if (!get_active_contract_count())
        {
            std::unique_lock uniqueLock(mutex_);
            conditionVariable_.wait_for(uniqueLock, maxWaitTime, [&]{return get_active_contract_count();});
        }
    }
    return process_contract();
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline std::size_t bcpp::system::work_contract_group<T>::execute_next_contract
(
)
{
    if constexpr (waitable)
    {
        if (!get_active_contract_count())
        {
            std::unique_lock uniqueLock(mutex_);
            conditionVariable_.wait(uniqueLock, [&]{return ((stopped_) || (get_active_contract_count()));});
        }
    }
    return process_contract();
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline std::size_t bcpp::system::work_contract_group<T>::process_contract
(
)
{
    static auto constexpr right = decrement_preference::right;
    static auto constexpr left = decrement_preference::left;

    std::uint64_t preferenceFlags = preferenceFlags_++;
    if (auto parent = (preferenceFlags & 1) ? decrement_contract_count<right>(0) : decrement_contract_count<left>(0))   
    {
        while (parent < firstContractIndex_) 
        {
            parent = (parent * 2) + ((preferenceFlags & 1) ? decrement_contract_count<right>(parent) : decrement_contract_count<left>(parent));
            preferenceFlags >>= 1;
        }
        process_contract(parent);
    }
    return invocationCounter_[0].get_count();
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
template <bcpp::system::work_contract_group<T>::decrement_preference T_>
inline std::int64_t bcpp::system::work_contract_group<T>::decrement_contract_count
(
    std::int64_t parent
)
{
    static auto constexpr left_preference = (T_ == decrement_preference::left);
    static auto constexpr mask = (left_preference) ? left_mask : right_mask;
    static auto constexpr prefered_addend = (left_preference) ? left_addend : right_addend;
    static auto constexpr fallback_addend = (left_preference) ? right_addend : left_addend;

    auto & invocationCounter = invocationCounter_[parent].u64_;
    auto expected = invocationCounter.load();
    auto addend = (expected & mask) ? prefered_addend : fallback_addend;
    while ((expected != 0) && (!invocationCounter.compare_exchange_strong(expected, expected - addend)))
        addend = (expected & mask) ? prefered_addend : fallback_addend;
    return expected ? (1 + (addend > left_mask)) : 0;
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline void bcpp::system::work_contract_group<T>::process_contract
(
    std::int64_t parent
)
{
    auto contractId = (parent - firstContractIndex_);
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
        std::lock_guard lockGuard(mutex_);
        flags = nextAvail_.load();
        nextAvail_ = contractId;
        contract.work_ = nullptr;
        surrenderToken_[contractId] = {};
    }
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline std::size_t bcpp::system::work_contract_group<T>::get_capacity
(
) const
{
    return contracts_.size();
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline bcpp::system::work_contract_group<T>::surrender_token::surrender_token
(
    work_contract_group * workContractGroup
):
    workContractGroup_(workContractGroup)
{
}


//=============================================================================
template <bcpp::system::work_contract_mode T>
inline bool bcpp::system::work_contract_group<T>::surrender_token::invoke
(
    work_contract_type const & workContract
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
template <bcpp::system::work_contract_mode T>
inline void bcpp::system::work_contract_group<T>::surrender_token::orphan
(
)
{
    std::lock_guard lockGuard(mutex_);
    workContractGroup_ = nullptr;
}
