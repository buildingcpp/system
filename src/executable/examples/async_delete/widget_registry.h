#pragma once

#include <cstddef>
#include <iostream>
#include <chrono>
#include <functional>
#include <thread>
#include <mutex>
#include <map>
#include <atomic>

#include <include/non_copyable.h>
#include <include/non_movable.h>
#include <library/system.h>

class widget_impl;


//=============================================================================
class widget_registry : bcpp::non_movable, bcpp::non_copyable
{
public:

    std::uint32_t register_widget
    (
        widget_impl *
    );

    void unregister_widget
    (
        std::uint32_t
    );

    void widge_all();

private:

    std::mutex                             mutex_;
    std::map<std::uint32_t, widget_impl *> widgets_; 
    std::atomic<std::uint32_t>             id_;
};
