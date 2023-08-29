#pragma once


namespace bcpp::system::internal
{

    enum class work_contract_mode : bool
    {
        non_blocking    = false,
        blocking        = true
    };

}