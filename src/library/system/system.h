#pragma once

#include "./cpu_id.h"
#include "./threading/thread_pool.h"
#include "./memory/shared_memory.h"
#include "./memory/memory_mapping.h"


namespace bcpp::system
{

    cpu_id get_cpu_affinity();

    bool set_cpu_affinity
    (
        cpu_id 
    );

} // namespace bcpp::system