#include "./sub_work_contract_group.h"


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
void bcpp::system::internal::sub_work_contract_group<T>::process_release
(
    std::int64_t contractId
)
{
    auto & contract = contracts_[contractId];
    if (contract.release_)
        std::exchange(contract.release_, nullptr)();
    contract.work_ = nullptr;
    releaseToken_[contractId] = {};

    availableFlag_[contractId >> 6] |= ((0x8000000000000000ull) >> (contractId & 63));
    contractId >>= 6;
    contractId += firstContractIndex_;
    while (contractId)
    {
        auto addend = ((contractId-- & 1ull) ? left_addend : right_addend);
        availableCounter_[contractId >>= 1].u64_ += addend;
    }
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
bcpp::system::internal::sub_work_contract_group<T>::release_token::release_token
(
    sub_work_contract_group * workContractGroup
):
    workContractGroup_(workContractGroup)
{
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
bool bcpp::system::internal::sub_work_contract_group<T>::release_token::schedule
(
    work_contract_type const & workContract
)
{
    std::lock_guard lockGuard(mutex_);
    if (auto workContractGroup = std::exchange(workContractGroup_, nullptr); workContractGroup != nullptr)
    {
        workContractGroup->release(workContract);
        return true;
    }
    return false;
}


//=============================================================================
template <bcpp::system::internal::work_contract_mode T>
void bcpp::system::internal::sub_work_contract_group<T>::release_token::orphan
(
)
{
    std::lock_guard lockGuard(mutex_);
    workContractGroup_ = nullptr;
}


//=============================================================================
namespace bcpp::system::internal
{
    template class sub_work_contract_group<work_contract_mode::blocking>;
    template class sub_work_contract_group<work_contract_mode::non_blocking>;
}