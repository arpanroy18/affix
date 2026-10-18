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

