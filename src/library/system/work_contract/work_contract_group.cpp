#include "./work_contract_group.h"



//=============================================================================
void __attribute__ ((noinline)) bcpp::system::work_contract_group::process_surrender
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