#if defined(__linux__)

#include "platform.hpp"
#include <sched.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <linux/perf_event.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <cstring>
#include <chrono>
#include <array>

namespace affix::internal {

namespace {

cpu_set_t cpuset_to_linux(const CpuSet& set) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (size_t i = 0; i < MAX_CPUS; ++i) {
        if (set.test(i)) {
            CPU_SET(i, &cpuset);
        }
    }
    return cpuset;
}

CpuSet linux_to_cpuset(const cpu_set_t& cpuset) {
    CpuSet set;
    for (size_t i = 0; i < MAX_CPUS; ++i) {
        if (CPU_ISSET(i, &cpuset)) {
            set.set(i);
        }
    }
    return set;
}

std::string read_sysfs_string(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::string val;
    std::getline(file, val);
    // trim newline
    while (!val.empty() && (val.back() == '\n' || val.back() == '\r')) {
        val.pop_back();
    }
    return val;
}

int64_t read_sysfs_int(const std::string& path, int64_t default_val = -1) {
    std::string s = read_sysfs_string(path);
    if (s.empty()) return default_val;
    try {
        return std::stoll(s);
    } catch (...) {
        return default_val;
    }
}

// Thread-local perf event file descriptors
struct PerfCounters {
    int fd_cycles = -1;
    int fd_instr = -1;
    int fd_l1_miss = -1;
    int fd_llc_miss = -1;
    bool attempted = false;
    bool available = false;

    static long perf_event_open(struct perf_event_attr* hw_event, pid_t pid,
                                int cpu, int group_fd, unsigned long flags) {
        return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
    }

    void init() {
        if (attempted) return;
        attempted = true;

        auto open_counter = [](uint32_t type, uint64_t config, int group_fd) -> int {
            struct perf_event_attr pe;
            std::memset(&pe, 0, sizeof(pe));
            pe.type = type;
            pe.size = sizeof(struct perf_event_attr);
            pe.config = config;
            pe.disabled = 0;
            pe.exclude_kernel = 1;
            pe.exclude_hv = 1;
            return static_cast<int>(perf_event_open(&pe, 0, -1, group_fd, 0));
        };

        fd_cycles = open_counter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES, -1);
        if (fd_cycles < 0) {
            available = false;
            return;
        }

        fd_instr = open_counter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS, fd_cycles);
        fd_l1_miss = open_counter(PERF_TYPE_HW_CACHE, 
            (PERF_COUNT_HW_CACHE_L1D) | (PERF_COUNT_HW_CACHE_OP_READ << 8) | (PERF_COUNT_HW_CACHE_RESULT_MISS << 16), 
            fd_cycles);
        fd_llc_miss = open_counter(PERF_TYPE_HARDWARE, PERF_COUNT_HW_CACHE_MISSES, fd_cycles);

        available = true;
    }

    void read_counters(uint64_t& cycles, uint64_t& instr, uint64_t& l1_miss, uint64_t& llc_miss) const {
        if (!available) return;
        auto read_fd = [](int fd) -> uint64_t {
            if (fd < 0) return 0;
            uint64_t val = 0;
            ssize_t res = read(fd, &val, sizeof(val));
            return (res == sizeof(val)) ? val : 0;
        };
        cycles = read_fd(fd_cycles);
        instr = read_fd(fd_instr);
        l1_miss = read_fd(fd_l1_miss);
        llc_miss = read_fd(fd_llc_miss);
    }

    ~PerfCounters() {
        if (fd_cycles >= 0) close(fd_cycles);
        if (fd_instr >= 0) close(fd_instr);
        if (fd_l1_miss >= 0) close(fd_l1_miss);
        if (fd_llc_miss >= 0) close(fd_llc_miss);
    }
};

thread_local PerfCounters tl_perf;

} // namespace

bool platform_set_thread_affinity(pthread_t handle, const CpuSet& mask, CpuSet* out_effective, std::string* error_msg) {
    cpu_set_t cpuset = cpuset_to_linux(mask);
    int res = pthread_setaffinity_np(handle, sizeof(cpu_set_t), &cpuset);
    if (res != 0) {
        if (error_msg) *error_msg = strerror(res);
        return false;
    }

    if (out_effective) {
        cpu_set_t effective;
        if (pthread_getaffinity_np(handle, sizeof(cpu_set_t), &effective) == 0) {
            *out_effective = linux_to_cpuset(effective);
        } else {
            *out_effective = mask;
        }
    }
    return true;
}

bool platform_get_thread_affinity(pthread_t handle, CpuSet& mask, std::string* error_msg) {
    cpu_set_t cpuset;
    int res = pthread_getaffinity_np(handle, sizeof(cpu_set_t), &cpuset);
    if (res != 0) {
        if (error_msg) *error_msg = strerror(res);
        return false;
    }
    mask = linux_to_cpuset(cpuset);
    return true;
}

bool platform_set_current_thread_affinity(const CpuSet& mask, CpuSet* out_effective, std::string* error_msg) {
    return platform_set_thread_affinity(pthread_self(), mask, out_effective, error_msg);
}

bool platform_get_current_thread_affinity(CpuSet& mask, std::string* error_msg) {
    return platform_get_thread_affinity(pthread_self(), mask, error_msg);
}

bool platform_set_tid_affinity(uint64_t tid, const CpuSet& mask, CpuSet* out_effective, std::string* error_msg) {
    cpu_set_t cpuset = cpuset_to_linux(mask);
    int res = sched_setaffinity(static_cast<pid_t>(tid), sizeof(cpu_set_t), &cpuset);
    if (res != 0) {
        if (error_msg) *error_msg = strerror(errno);
        return false;
    }
    if (out_effective) {
        cpu_set_t effective;
        if (sched_getaffinity(static_cast<pid_t>(tid), sizeof(cpu_set_t), &effective) == 0) {
            *out_effective = linux_to_cpuset(effective);
        } else {
            *out_effective = mask;
        }
    }
    return true;
}

bool platform_set_thread_priority(pthread_t handle, SchedPriority priority, int val, std::string* error_msg) {
    int policy = SCHED_OTHER;
    struct sched_param param;
    std::memset(&param, 0, sizeof(param));

    switch (priority) {
        case SchedPriority::RealtimeFifo:
            policy = SCHED_FIFO;
            param.sched_priority = (val >= 1 && val <= 99) ? val : 50;
            break;
        case SchedPriority::RealtimeRoundRobin:
            policy = SCHED_RR;
            param.sched_priority = (val >= 1 && val <= 99) ? val : 50;
            break;
#ifdef SCHED_BATCH
        case SchedPriority::Batch:
            policy = SCHED_BATCH;
            param.sched_priority = 0;
            break;
#endif
#ifdef SCHED_IDLE
        case SchedPriority::Idle:
            policy = SCHED_IDLE;
            param.sched_priority = 0;
            break;
#endif
        case SchedPriority::Default:
        default:
            policy = SCHED_OTHER;
            param.sched_priority = 0;
            break;
    }

    int res = pthread_setschedparam(handle, policy, &param);
    if (res != 0) {
        if (error_msg) {
            *error_msg = (res == EPERM) 
                ? "Setting real-time priority requires CAP_SYS_NICE or root permissions" 
                : strerror(res);
        }
        return false;
    }
    return true;
}

bool platform_get_thread_priority(pthread_t handle, SchedPriority& priority, int& val, std::string* error_msg) {
    int policy = 0;
    struct sched_param param;
    int res = pthread_getschedparam(handle, &policy, &param);
    if (res != 0) {
        if (error_msg) *error_msg = strerror(res);
        return false;
    }
    val = param.sched_priority;
    if (policy == SCHED_FIFO) priority = SchedPriority::RealtimeFifo;
    else if (policy == SCHED_RR) priority = SchedPriority::RealtimeRoundRobin;
#ifdef SCHED_BATCH
    else if (policy == SCHED_BATCH) priority = SchedPriority::Batch;
#endif
#ifdef SCHED_IDLE
    else if (policy == SCHED_IDLE) priority = SchedPriority::Idle;
#endif
    else priority = SchedPriority::Default;
    return true;
}

bool platform_set_current_thread_priority(SchedPriority priority, int val, std::string* error_msg) {
    return platform_set_thread_priority(pthread_self(), priority, val, error_msg);
}

bool platform_get_current_thread_priority(SchedPriority& priority, int& val, std::string* error_msg) {
    return platform_get_thread_priority(pthread_self(), priority, val, error_msg);
}

uint32_t platform_get_current_cpu() {
    int cpu = sched_getcpu();
    return (cpu >= 0) ? static_cast<uint32_t>(cpu) : 0;
}

uint64_t platform_get_current_tid() {
    return static_cast<uint64_t>(syscall(SYS_gettid));
}

void platform_discover_topology(SystemTopology& topo) {
    // 1. Process allowed CPUs
    cpu_set_t current_mask;
    if (sched_getaffinity(0, sizeof(cpu_set_t), &current_mask) == 0) {
        topo.set_allowed_cpus(linux_to_cpuset(current_mask));
    }

    // 1b. Discovered isolated CPUs from /sys/devices/system/cpu/isolated
    std::string isolated_str = read_sysfs_string("/sys/devices/system/cpu/isolated");
    CpuSet isolated_set;
    if (!isolated_str.empty()) {
        isolated_set = CpuSet::from_string(isolated_str);
        topo.set_isolated_cpus(isolated_set);
    }

    // 2. Discover CPUs from /sys/devices/system/cpu/
    namespace fs = std::filesystem;
    std::string base_path = "/sys/devices/system/cpu";
    if (!fs::exists(base_path)) return;

    for (const auto& entry : fs::directory_iterator(base_path)) {
        std::string filename = entry.path().filename().string();
        if (filename.rfind("cpu", 0) != 0) continue;
        std::string num_str = filename.substr(3);
        if (num_str.empty() || !std::isdigit(num_str[0])) continue;

        uint32_t logical_id = std::stoul(num_str);
        CpuCore core;
        core.logical_id = logical_id;

        std::string topo_dir = entry.path().string() + "/topology";
        core.physical_id = static_cast<uint32_t>(read_sysfs_int(topo_dir + "/core_id", logical_id));
        core.package_id = static_cast<uint32_t>(read_sysfs_int(topo_dir + "/physical_package_id", 0));

        std::string siblings_str = read_sysfs_string(topo_dir + "/thread_siblings_list");
        if (!siblings_str.empty()) {
            core.smt_siblings = CpuSet::from_string(siblings_str);
        } else {
            core.smt_siblings.set(logical_id);
        }

        // Cache information
        std::string cache_base = entry.path().string() + "/cache";
        if (fs::exists(cache_base)) {
            for (const auto& cache_entry : fs::directory_iterator(cache_base)) {
                std::string cname = cache_entry.path().filename().string();
                if (cname.rfind("index", 0) != 0) continue;

                CacheInfo cinfo;
                cinfo.level = static_cast<uint32_t>(read_sysfs_int(cache_entry.path().string() + "/level", 1));
                cinfo.type = read_sysfs_string(cache_entry.path().string() + "/type");
                
                std::string size_str = read_sysfs_string(cache_entry.path().string() + "/size");
                if (!size_str.empty()) {
                    uint64_t mult = 1;
                    if (size_str.back() == 'K') { mult = 1024; size_str.pop_back(); }
                    else if (size_str.back() == 'M') { mult = 1024 * 1024; size_str.pop_back(); }
                    cinfo.size_bytes = std::stoull(size_str) * mult;
                }

                std::string shared_str = read_sysfs_string(cache_entry.path().string() + "/shared_cpu_list");
                if (!shared_str.empty()) {
                    cinfo.shared_cpus = CpuSet::from_string(shared_str);
                }
                core.caches.push_back(cinfo);
            }
        }

        // Core type detection (P vs E on Linux)
        int64_t capacity = read_sysfs_int(entry.path().string() + "/cpu_capacity", -1);
        if (capacity > 0) {
            if (capacity >= 1024) core.core_type = CoreType::Performance;
            else core.core_type = CoreType::Efficient;
        }

        core.is_isolated = isolated_set.test(logical_id);

        topo.add_core(core);
    }

    // 3. NUMA discovery from /sys/devices/system/node/
    std::string numa_base = "/sys/devices/system/node";
    if (fs::exists(numa_base)) {
        for (const auto& entry : fs::directory_iterator(numa_base)) {
            std::string filename = entry.path().filename().string();
            if (filename.rfind("node", 0) != 0) continue;
            std::string num_str = filename.substr(4);
            if (num_str.empty() || !std::isdigit(num_str[0])) continue;

            NumaNode node;
            node.id = static_cast<NumaId>(std::stoi(num_str));
            std::string cpulist_str = read_sysfs_string(entry.path().string() + "/cpulist");
            node.cpus = CpuSet::from_string(cpulist_str);

            topo.add_numa_node(node);
        }
    }
}

MetricsSnapshot platform_capture_thread_metrics() {
    MetricsSnapshot s;

    // Wall-clock time
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    s.wall_time_ns = static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL + static_cast<uint64_t>(ts.tv_nsec);

    // Thread CPU time
    struct timespec ts_cpu;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts_cpu);
    s.cpu_time_ns = static_cast<uint64_t>(ts_cpu.tv_sec) * 1'000'000'000ULL + static_cast<uint64_t>(ts_cpu.tv_nsec);

    // Context switches via getrusage(RUSAGE_THREAD)
    struct rusage usage;
    if (getrusage(RUSAGE_THREAD, &usage) == 0) {
        s.voluntary_context_switches = usage.ru_nvcsw;
        s.involuntary_context_switches = usage.ru_nivcsw;
    }

    s.starting_core = platform_get_current_cpu();
    s.ending_core = s.starting_core;

    // Hardware PMU counters
    tl_perf.init();
    if (tl_perf.available) {
        s.hardware_counters_available = true;
        tl_perf.read_counters(s.cycles, s.instructions, s.l1_cache_misses, s.llc_cache_misses);
    }

    return s;
}

bool platform_bind_memory_to_numa(void* ptr, size_t size, NumaId node, std::string* error_msg) {
    if (!ptr || size == 0) return true;
    if (node < 0) return true;

    unsigned long nodemask = (1UL << node);
    constexpr int MPOL_BIND = 2;
    constexpr unsigned MPOL_MF_MOVE = (1 << 1);

#if defined(SYS_mbind)
    long res = syscall(SYS_mbind, ptr, size, MPOL_BIND, &nodemask, sizeof(nodemask) * 8, MPOL_MF_MOVE);
    if (res != 0) {
        if (error_msg) *error_msg = strerror(errno);
        return false;
    }
    return true;
#else
    if (error_msg) *error_msg = "mbind syscall not supported on this architecture";
    return false;
#endif
}

bool platform_set_thread_preferred_numa(NumaId node, std::string* error_msg) {
    if (node < 0) return true;
    unsigned long nodemask = (1UL << node);
    constexpr int MPOL_PREFERRED = 1;

#if defined(SYS_set_mempolicy)
    long res = syscall(SYS_set_mempolicy, MPOL_PREFERRED, &nodemask, sizeof(nodemask) * 8);
    if (res != 0) {
        if (error_msg) *error_msg = strerror(errno);
        return false;
    }
    return true;
#else
    if (error_msg) *error_msg = "set_mempolicy syscall not supported on this architecture";
    return false;
#endif
}

void* platform_allocate_numa(size_t size, NumaId node) {
    void* ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) return nullptr;

    if (node >= 0) {
        platform_bind_memory_to_numa(ptr, size, node, nullptr);
    }
    return ptr;
}

void platform_free_numa(void* ptr, size_t size) {
    if (ptr && ptr != MAP_FAILED) {
        munmap(ptr, size);
    }
}

} // namespace affix::internal

#endif // __linux__
