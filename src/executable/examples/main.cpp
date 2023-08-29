/*
#include <cstddef>
#include <iostream>
#include <memory>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <cstdint>
#include <atomic>
#include <vector>
#include <thread>
#include <cmath>
#include <iomanip>

#include <include/non_copyable.h>
#include <include/non_movable.h>

#include <library/system.h>

class widget;
class widget_impl;


class widget_manager
{
public:

    void register_widget
    (
        widget_impl * widget
    )
    {
        std::lock_guard lockGuard(mutex_);
        widgetMap_[widget->get_id()] = widget;
    }

private:

    std::mutex mutex_;
    std::map<widget_impl::id_type, widget_impl *> widgetMap_;
};


//=============================================================================
class widget_impl :
    non_movable,
    non_copyable
{
public:

    using id_type = int;

    widget
    (
        bcpp::system::non_blocking_work_contract_group & workContractGroup
    ):
        workContract_(workContractGroup.create_contract(
            [this](){this->foo();},
            [this](){this->destroy();})),
        id_(nextId_++)
    {
    }

    void get_id() const{return id_;}

private:

    friend class widget;

    void foo(){std::cout << "foo\n";}

    void destroy()
    {
        if (workContract_.is_valid())
        {
            workContract_.release(); // flag for asynch destruction
        }
        else
        {
            // do clean up prior to delete
            delete this;
        }   
    }

    bcpp::system::non_blocking_work_contract workContract_;
    id_type id_;
    static std::atomic<id_type> nextId_;
};
std::atomic<widget_impl::id_type> widget_impl::::nextId_;


//=============================================================================
class widget
{
public:

    using impl_type = widget_impl;

    widget
    (
        bcpp::system::non_blocking_work_contract_group & workContractGroup
    ):
        impl_(std::make_unique<impl_type>(workContractGroup, [](auto * impl){impl->destroy();}))
    {
    }

private:

    std::unique_ptr<impl_type, void(*)(impl_type)> impl_;
};

//=============================================================================
  int main()
  {
      bcpp::system::non_blocking_work_contract_group workContractGroup(256);
      workContract = std::move(workContractGroup.create_contract(work_function, release_function));
      std::jthread workerThread([&](){while (!releaseed) workContractGroup.execute_next_contract();});
      
      workContract.schedule();
      while (!releaseed);
      return 0;
  }
*/