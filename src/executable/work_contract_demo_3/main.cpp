#include <cstddef>
#include <iostream>
#include <condition_variable>
#include <mutex>
#include <cstdint>
#include <memory>
#include <thread>

#include <library/system.h>


//=============================================================================
class widget_impl
{
public:

    widget_impl
    (
        bcpp::system::work_contract_group & workContractGroup
    ):
        workContract_(workContractGroup.create_contract([this](){this->do_stuff();}, [this](){this->destroy();})) 
    {
    }

    void do_stuff(){}

    ~widget_impl(){std::cout << "impl dtor\n";}

    void destroy
    (
    )
    {
        if (workContract_.is_valid())
            workContract_.surrender();
        else
            delete this;
    }

    bcpp::system::work_contract workContract_;
};


//=============================================================================
class widget :
bcpp::non_copyable
{
public:
    using impl_type = widget_impl;
    widget(bcpp::system::work_contract_group & workContractGroup)
    {
        impl_ = std::move(decltype(impl_)(new impl_type(workContractGroup), [](auto * impl){impl->destroy();}));
    }
    widget(widget &&) = default;
    widget & operator = (widget &&) = default;
private:
    std::unique_ptr<impl_type, std::function<void(impl_type *)>>   impl_;
};


//=============================================================================
int main
(
    int, 
    char const **
)
{
    // work contract group with 1024 available contracts
    bcpp::system::work_contract_group workContractGroup(1024);

    // thread pool with a single worker thread
    std::vector<bcpp::system::thread_pool::thread_configuration> threadConfig;
    threadConfig.push_back({.function_ = [&](std::stop_token stopToken)
                    {
                        while (!stopToken.stop_requested()) 
                            workContractGroup.service_contracts();
                    }});
    bcpp::system::thread_pool threadPool({threadConfig});

    {
        // create a widget and then let it go out of scope
        widget w(workContractGroup);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    // ensure that the async dtor of the widget has time to be invoked prior to exit
    // otherwise output generated during dtor might not get produced for the demo.
    std::this_thread::sleep_for(std::chrono::seconds(1));

    return 0;
}
