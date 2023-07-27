#include "./system.h"


//==============================================================================
auto bcpp::system::get_cpu_affinity
(
) -> cpu_id
{
    #ifdef __linux__
    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);

    pthread_getaffinity_np(pthread_self(), sizeof(cpuSet), &cpuSet);
    for (int32_t i = 0; i < CPU_SETSIZE; ++i)
        if (CPU_ISSET(i, &cpuSet))
            return cpu_id(i);
    #endif
    return cpu_id(-1);
}


//==============================================================================
bool bcpp::system::set_cpu_affinity
(
    // set core affinity for the thread
    cpu_id cpuId
)
{
    #ifdef __linux__
    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
    CPU_SET(cpuId, &cpuSet);
    return (pthread_setaffinity_np(pthread_self(), sizeof(cpuSet), &cpuSet) == 0);
    #endif

    return false;
}
