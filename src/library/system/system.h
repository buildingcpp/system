#pragma once

#include "./cpu_id.h"
#include "./threading/thread_pool.h"
#include "./work_contract/work_contract_group.h"


namespace bcpp::system
{

    cpu_id get_cpu_affinity();

    bool set_cpu_affinity
    (
        cpu_id 
    );

} // namespace bcpp::system