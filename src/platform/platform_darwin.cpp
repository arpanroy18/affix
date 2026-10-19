#if defined(__APPLE__)

#include "platform.hpp"
#include <pthread.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#include <sys/sysctl.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <chrono>

namespace affix::internal {

namespace {

int get_sysctl_int(const char* name, int default_val = 0) {
    int val = 0;
    size_t len = sizeof(val);
    if (sysctlbyname(name, &val, &len, nullptr, 0) == 0) {
        return val;
    }
    return default_val;
}

int64_t get_sysctl_int64(const char* name, int64_t default_val = 0) {
    int64_t val = 0;
    size_t len = sizeof(val);
    if (sysctlbyname(name, &val, &len, nullptr, 0) == 0) {
        return val;
    }
    return default_val;
}

// Stores thread affinity tag set via Mach thread policy
thread_local CpuSet tl_current_affinity;
thread_local bool tl_has_set_affinity = false;
thread_local SchedPriority tl_current_priority = SchedPriority::Default;
thread_local int tl_current_priority_val = 0;

} // namespace

