#include "./widget_registry.h"
#include "./widget_impl.h"


//=============================================================================
std::uint32_t widget_registry::register_widget
(
    widget_impl * widget
)
{
    std::lock_guard lockGuard(mutex_);
    std::uint32_t id = ++id_;
    widgets_[id] = widget;
    std::cout << "Registered widget " << id << "\n";
    return id;
}


//=============================================================================
void widget_registry::unregister_widget
(
    std::uint32_t id
)
{
    std::lock_guard lockGuard(mutex_);
    if (auto iter = widgets_.find(id); iter != widgets_.end())
    {
        widgets_.erase(iter);
        std::cout << "unregistered widget " << id << "\n";
    }
}


//=============================================================================
void widget_registry::widge_all()
{
    std::lock_guard lockGuard(mutex_);
    for (auto && [id, widget] : widgets_)
        widget->widge();
}
