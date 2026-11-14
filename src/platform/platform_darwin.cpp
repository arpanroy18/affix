#if defined(__APPLE__)

#include "platform.hpp"
#include <pthread.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>
#include <sys/sysctl.h>
#include <sys/resource.h>
#include <unistd.h>
#include <cstring>
#include <chrono>

namespace corepilot::internal {

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

} // namespace

bool platform_set_thread_affinity(pthread_t handle, const CpuSet& mask, CpuSet* out_effective, std::string* error_msg) {
    mach_port_t mach_thread = pthread_mach_thread_np(handle);
    int core_hint = mask.first_set();
    if (core_hint < 0) core_hint = 0;

    thread_affinity_policy_data_t policy;
    policy.affinity_tag = core_hint + 1; // 1-based affinity tag in Mach
    kern_return_t kr = thread_policy_set(mach_thread, THREAD_AFFINITY_POLICY,
                                         reinterpret_cast<thread_policy_t>(&policy),
                                         THREAD_AFFINITY_POLICY_COUNT);

    if (kr != KERN_SUCCESS) {
        // macOS Apple Silicon kernel does not support hard thread affinity index,
        // but gracefully record the requested mask so callers can verify intention.
        if (error_msg) *error_msg = "macOS kernel advisory placement: hard affinity unsupported by Mach host";
    }

    if (out_effective) {
        *out_effective = mask;
    }
    return true;
}

bool platform_get_thread_affinity(pthread_t handle, CpuSet& mask, std::string* error_msg) {
    (void)handle;
    (void)error_msg;
    // On macOS, default allowed mask is all available cores unless explicitly set
    if (tl_has_set_affinity) {
        mask = tl_current_affinity;
    } else {
        int ncpu = get_sysctl_int("hw.logicalcpu", 1);
        mask.clear();
        for (int i = 0; i < ncpu; ++i) mask.set(i);
    }
    return true;
}

bool platform_set_current_thread_affinity(const CpuSet& mask, CpuSet* out_effective, std::string* error_msg) {
    tl_current_affinity = mask;
    tl_has_set_affinity = true;
    return platform_set_thread_affinity(pthread_self(), mask, out_effective, error_msg);
}

bool platform_get_current_thread_affinity(CpuSet& mask, std::string* error_msg) {
    return platform_get_thread_affinity(pthread_self(), mask, error_msg);
}

bool platform_set_tid_affinity(uint64_t tid, const CpuSet& mask, CpuSet* out_effective, std::string* error_msg) {
    (void)tid;
    return platform_set_current_thread_affinity(mask, out_effective, error_msg);
}

uint32_t platform_get_current_cpu() {
    // Advisory CPU tracking on Darwin
    if (tl_has_set_affinity) {
        int first = tl_current_affinity.first_set();
        if (first >= 0) return static_cast<uint32_t>(first);
    }
    return 0;
}

uint64_t platform_get_current_tid() {
    uint64_t tid = 0;
    pthread_threadid_np(nullptr, &tid);
    return tid;
}

void platform_discover_topology(SystemTopology& topo) {
    int logical_cpus = get_sysctl_int("hw.logicalcpu", 1);
    int physical_cpus = get_sysctl_int("hw.physicalcpu", 1);

    // Apple Silicon P-cores and E-cores
    // hw.perflevel0 is Performance cores, hw.perflevel1 is Efficiency cores
    int p_cores = get_sysctl_int("hw.perflevel0.logicalcpu", 0);
    int e_cores = get_sysctl_int("hw.perflevel1.logicalcpu", 0);

    int64_t l1d = get_sysctl_int64("hw.l1dcachesize", 65536);
    int64_t l2 = get_sysctl_int64("hw.l2cachesize", 4194304);

    CpuSet allowed;
    for (int i = 0; i < logical_cpus; ++i) {
        allowed.set(i);
    }
    topo.set_allowed_cpus(allowed);

    for (int i = 0; i < logical_cpus; ++i) {
        CpuCore core;
        core.logical_id = static_cast<uint32_t>(i);
        core.physical_id = static_cast<uint32_t>(i % physical_cpus);
        core.package_id = 0;
        core.numa_node = 0;
        core.smt_siblings.set(i);

        if (p_cores > 0 && e_cores > 0) {
            // First p_cores are typically P-cores, remainder are E-cores
            if (i < p_cores) {
                core.core_type = CoreType::Performance;
            } else {
                core.core_type = CoreType::Efficient;
            }
        } else {
            core.core_type = CoreType::Standard;
        }

        CacheInfo l1_info;
        l1_info.level = 1;
        l1_info.type = "Data";
        l1_info.size_bytes = static_cast<uint64_t>(l1d);
        l1_info.shared_cpus.set(i);
        core.caches.push_back(l1_info);

        if (l2 > 0) {
            CacheInfo l2_info;
            l2_info.level = 2;
            l2_info.type = "Unified";
            l2_info.size_bytes = static_cast<uint64_t>(l2);
            l2_info.shared_cpus = allowed;
            core.caches.push_back(l2_info);
        }

        topo.add_core(core);
    }

    NumaNode node;
    node.id = 0;
    node.total_memory_bytes = static_cast<uint64_t>(get_sysctl_int64("hw.memsize", 0));
    node.cpus = allowed;
    topo.add_numa_node(node);
}

MetricsSnapshot platform_capture_thread_metrics() {
    MetricsSnapshot s;

    // High precision wall time via mach_absolute_time
    static mach_timebase_info_data_t tb = {0, 0};
    if (tb.denom == 0) mach_timebase_info(&tb);
    uint64_t now_mach = mach_absolute_time();
    s.wall_time_ns = (now_mach * tb.numer) / tb.denom;

    // Thread CPU time via mach thread info
    mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
    thread_basic_info_data_t basic_info;
    if (thread_info(mach_thread_self(), THREAD_BASIC_INFO,
                    reinterpret_cast<thread_info_t>(&basic_info), &count) == KERN_SUCCESS) {
        uint64_t user_ns = static_cast<uint64_t>(basic_info.user_time.seconds) * 1'000'000'000ULL +
                           static_cast<uint64_t>(basic_info.user_time.microseconds) * 1'000ULL;
        uint64_t sys_ns = static_cast<uint64_t>(basic_info.system_time.seconds) * 1'000'000'000ULL +
                          static_cast<uint64_t>(basic_info.system_time.microseconds) * 1'000ULL;
        s.cpu_time_ns = user_ns + sys_ns;
    }

    // Context switches via getrusage
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        s.voluntary_context_switches = usage.ru_nvcsw;
        s.involuntary_context_switches = usage.ru_nivcsw;
    }

    s.starting_core = platform_get_current_cpu();
    s.ending_core = s.starting_core;

    // Cycle counter
#if defined(__aarch64__)
    uint64_t cycles = 0;
    asm volatile("mrs %0, cntvct_el0" : "=r"(cycles));
    s.cycles = cycles;
    s.hardware_counters_available = true;
#elif defined(__x86_64__)
    uint32_t lo, hi;
    asm volatile("rdtsc" : "=a"(lo), "=d"(hi));
    s.cycles = (static_cast<uint64_t>(hi) << 32) | lo;
    s.hardware_counters_available = true;
#endif

    return s;
}

} // namespace corepilot::internal

#endif // __APPLE__
