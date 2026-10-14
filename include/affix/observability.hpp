#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include <chrono>

namespace affix {

struct MetricsSnapshot {
    uint64_t wall_time_ns = 0;
    uint64_t cpu_time_ns = 0;
    int64_t voluntary_context_switches = 0;
    int64_t involuntary_context_switches = 0;
    int64_t cpu_migrations = 0;
    uint32_t starting_core = 0;
    uint32_t ending_core = 0;

    // Hardware PMU counters
    uint64_t cycles = 0;
    uint64_t instructions = 0;
    uint64_t l1_cache_misses = 0;
    uint64_t llc_cache_misses = 0;
    bool hardware_counters_available = false;

    double ipc() const {
        if (!hardware_counters_available || cycles == 0) return 0.0;
        return static_cast<double>(instructions) / static_cast<double>(cycles);
    }

    double cpu_utilization() const {
        if (wall_time_ns == 0) return 0.0;
        return (static_cast<double>(cpu_time_ns) / static_cast<double>(wall_time_ns)) * 100.0;
    }

    MetricsSnapshot operator-(const MetricsSnapshot& other) const {
        MetricsSnapshot delta;
        delta.wall_time_ns = (wall_time_ns >= other.wall_time_ns) ? (wall_time_ns - other.wall_time_ns) : 0;
        delta.cpu_time_ns = (cpu_time_ns >= other.cpu_time_ns) ? (cpu_time_ns - other.cpu_time_ns) : 0;
        delta.voluntary_context_switches = voluntary_context_switches - other.voluntary_context_switches;
        delta.involuntary_context_switches = involuntary_context_switches - other.involuntary_context_switches;
        delta.cpu_migrations = cpu_migrations - other.cpu_migrations;
        delta.starting_core = other.starting_core;
        delta.ending_core = ending_core;

        if (hardware_counters_available && other.hardware_counters_available) {
            delta.hardware_counters_available = true;
            delta.cycles = (cycles >= other.cycles) ? (cycles - other.cycles) : 0;
            delta.instructions = (instructions >= other.instructions) ? (instructions - other.instructions) : 0;
            delta.l1_cache_misses = (l1_cache_misses >= other.l1_cache_misses) ? (l1_cache_misses - other.l1_cache_misses) : 0;
            delta.llc_cache_misses = (llc_cache_misses >= other.llc_cache_misses) ? (llc_cache_misses - other.llc_cache_misses) : 0;
        } else {
            delta.hardware_counters_available = false;
        }
        return delta;
    }

    MetricsSnapshot& operator+=(const MetricsSnapshot& other) {
        wall_time_ns += other.wall_time_ns;
        cpu_time_ns += other.cpu_time_ns;
        voluntary_context_switches += other.voluntary_context_switches;
        involuntary_context_switches += other.involuntary_context_switches;
        cpu_migrations += other.cpu_migrations;
        if (other.hardware_counters_available) {
            hardware_counters_available = true;
            cycles += other.cycles;
            instructions += other.instructions;
            l1_cache_misses += other.l1_cache_misses;
            llc_cache_misses += other.llc_cache_misses;
        }
        return *this;
    }

    std::string to_string() const;
    std::string to_json() const;
};

MetricsSnapshot capture_current_thread_metrics();

class ScopedProbe {
public:
    explicit ScopedProbe(MetricsSnapshot* out_metrics = nullptr);
    explicit ScopedProbe(std::string name, std::function<void(const std::string&, const MetricsSnapshot&)> callback = nullptr);
    ~ScopedProbe();

    ScopedProbe(const ScopedProbe&) = delete;
    ScopedProbe& operator=(const ScopedProbe&) = delete;

    MetricsSnapshot elapsed() const;

private:
    std::string name_;
    MetricsSnapshot start_snapshot_;
    MetricsSnapshot* out_metrics_ = nullptr;
    std::function<void(const std::string&, const MetricsSnapshot&)> callback_;
};

struct LatencyStats {
    double min_ms = 0.0;
    double max_ms = 0.0;
    double mean_ms = 0.0;
    double p50_ms = 0.0;
    double p90_ms = 0.0;
    double p95_ms = 0.0;
    double p99_ms = 0.0;
    size_t sample_count = 0;
};

class TelemetrySession {
public:
    explicit TelemetrySession(std::string name);

    void record(const MetricsSnapshot& snapshot);
    void reset();

    size_t sample_count() const { return samples_.size(); }
    const std::string& name() const { return name_; }

    LatencyStats wall_time_stats() const;
    MetricsSnapshot aggregate_metrics() const;
    std::string summary() const;
    std::string to_json() const;

private:
    std::string name_;
    std::vector<MetricsSnapshot> samples_;
};

} // namespace affix
