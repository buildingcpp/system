#include "./work_contract_group.h"


//=============================================================================
std::size_t bcpp::system::work_contract_group::get_capacity
(
) const
{
    return contracts_.size();
}


//=============================================================================
void bcpp::system::work_contract_group::process_surrender
(
    std::int64_t contractId
)
{
    auto & contract = contracts_[contractId];
    if (contract.surrender_)
        std::exchange(contract.surrender_, nullptr)();
    contract.work_ = nullptr;
    surrenderToken_[contractId] = {};

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
bcpp::system::work_contract_group::surrender_token::surrender_token
(
    work_contract_group * workContractGroup
):
    workContractGroup_(workContractGroup)
{
}


//=============================================================================
bool bcpp::system::work_contract_group::surrender_token::invoke
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
void bcpp::system::work_contract_group::surrender_token::orphan
(
)
{
    std::lock_guard lockGuard(mutex_);
    workContractGroup_ = nullptr;
}
