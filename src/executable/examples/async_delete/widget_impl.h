#pragma once

#include <cstddef>
#include <iostream>

#include <include/non_copyable.h>
#include <include/non_movable.h>
#include <library/system.h>

class widget_registry;


//=============================================================================
class widget_impl : bcpp::non_movable, bcpp::non_copyable
{
public:

    widget_impl
    (
        widget_registry *, 
        bcpp::system::blocking_work_contract_group &
    );

    ~widget_impl();

    void destroy();

    void schedule_widge();

    void widge();

private:
    widget_registry *                   widgetRegistery_;
    std::uint32_t                        registrationId_;
    bcpp::system::blocking_work_contract workContract_;
};
