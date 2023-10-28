#pragma once

#include "./widget_impl.h"
#include "./widget_registry.h"

#include <library/system.h>

#include <cstddef>
#include <iostream>
#include <memory>


//=============================================================================
class widget
{
public:
    using impl_type = widget_impl;
    widget(widget_registry * widgetRegistery, bcpp::system::blocking_work_contract_group & workContractGroup):
            impl_(new impl_type(widgetRegistery, workContractGroup), [](auto * impl){impl->destroy();}){}
private:
    std::unique_ptr<impl_type, std::function<void(impl_type *)>> impl_;
};
