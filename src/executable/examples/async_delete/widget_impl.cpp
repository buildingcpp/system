#include "./widget_impl.h"
#include "./widget_registry.h"


//=============================================================================
widget_impl::widget_impl
(
    widget_registry * widgetRegistery, 
    bcpp::system::blocking_work_contract_group & wcg
):
    widgetRegistery_(widgetRegistery),
    registrationId_(widgetRegistery_->register_widget(this)),
    workContract_(wcg.create_contract([this](){this->widge();}, [this](){delete this;}))
{
}


//=============================================================================
widget_impl::~widget_impl
(
)
{
    std::cout << "~widget_impl\n";
}


//=============================================================================
void widget_impl::destroy
(
)
{
    widgetRegistery_->unregister_widget(registrationId_);
    workContract_.release();
}


//=============================================================================
void widget_impl::widge
(
)
{
    std::cout << "widge\n";
}


//=============================================================================
void widget_impl::schedule_widge
(
)
{
    workContract_.schedule();
}
