#include "affix/observability.hpp"
#include "../platform/platform.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>

namespace affix {

std::string MetricsSnapshot::to_string() const {
    std::ostringstream ss;
    double wall_ms = static_cast<double>(wall_time_ns) / 1'000'000.0;
    double cpu_ms = static_cast<double>(cpu_time_ns) / 1'000'000.0;

    ss << std::fixed << std::setprecision(3);
    ss << "Wall Time:        " << wall_ms << " ms\n";
    ss << "CPU Time:         " << cpu_ms << " ms\n";
    ss << "CPU Utilization:  " << std::setprecision(1) << cpu_utilization() << " %\n";
    ss << "Voluntary Switches:   " << voluntary_context_switches << "\n";
    ss << "Involuntary Switches: " << involuntary_context_switches << "\n";
    ss << "CPU Core Hop:         " << starting_core << " -> " << ending_core << "\n";

    if (hardware_counters_available) {
        ss << "Cycles:           " << cycles << "\n";
        ss << "Instructions:     " << instructions << "\n";
        ss << "IPC:              " << std::setprecision(2) << ipc() << "\n";
        ss << "L1 Cache Misses:  " << l1_cache_misses << "\n";
        ss << "LLC Cache Misses: " << llc_cache_misses << "\n";
    } else {
        ss << "PMU Counters:     Not available / unprivileged\n";
    }
    return ss.str();
}

std::string MetricsSnapshot::to_json() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);
    ss << "{\n";
    ss << "  \"wall_time_ms\": " << (static_cast<double>(wall_time_ns) / 1'000'000.0) << ",\n";
    ss << "  \"cpu_time_ms\": " << (static_cast<double>(cpu_time_ns) / 1'000'000.0) << ",\n";
    ss << "  \"cpu_utilization_pct\": " << cpu_utilization() << ",\n";
    ss << "  \"voluntary_context_switches\": " << voluntary_context_switches << ",\n";
    ss << "  \"involuntary_context_switches\": " << involuntary_context_switches << ",\n";
    ss << "  \"starting_core\": " << starting_core << ",\n";
    ss << "  \"ending_core\": " << ending_core << ",\n";
    ss << "  \"hardware_counters_available\": " << (hardware_counters_available ? "true" : "false") << ",\n";
    ss << "  \"cycles\": " << cycles << ",\n";
    ss << "  \"instructions\": " << instructions << ",\n";
    ss << "  \"ipc\": " << std::setprecision(2) << ipc() << ",\n";
    ss << "  \"l1_cache_misses\": " << l1_cache_misses << ",\n";
    ss << "  \"llc_cache_misses\": " << llc_cache_misses << "\n";
    ss << "}";
    return ss.str();
}

MetricsSnapshot capture_current_thread_metrics() {
    return internal::platform_capture_thread_metrics();
}

ScopedProbe::ScopedProbe(MetricsSnapshot* out_metrics)
    : out_metrics_(out_metrics) {
    start_snapshot_ = capture_current_thread_metrics();
}

ScopedProbe::ScopedProbe(std::string name, std::function<void(const std::string&, const MetricsSnapshot&)> callback)
    : name_(std::move(name)), callback_(std::move(callback)) {
    start_snapshot_ = capture_current_thread_metrics();
}

ScopedProbe::~ScopedProbe() {
    MetricsSnapshot end_snapshot = capture_current_thread_metrics();
    MetricsSnapshot delta = end_snapshot - start_snapshot_;

    if (out_metrics_) {
        *out_metrics_ = delta;
    }
    if (callback_) {
        callback_(name_, delta);
    }
}

MetricsSnapshot ScopedProbe::elapsed() const {
    MetricsSnapshot now = capture_current_thread_metrics();
    return now - start_snapshot_;
}

TelemetrySession::TelemetrySession(std::string name)
    : name_(std::move(name)) {}

void TelemetrySession::record(const MetricsSnapshot& snapshot) {
    samples_.push_back(snapshot);
}

void TelemetrySession::reset() {
    samples_.clear();
}

LatencyStats TelemetrySession::wall_time_stats() const {
    LatencyStats stats;
    if (samples_.empty()) return stats;

    std::vector<double> latencies;
    latencies.reserve(samples_.size());
    double sum = 0.0;

    for (const auto& s : samples_) {
        double ms = static_cast<double>(s.wall_time_ns) / 1'000'000.0;
        latencies.push_back(ms);
        sum += ms;
    }

    std::sort(latencies.begin(), latencies.end());

    stats.sample_count = latencies.size();
    stats.min_ms = latencies.front();
    stats.max_ms = latencies.back();
    stats.mean_ms = sum / static_cast<double>(latencies.size());

    auto percentile = [&](double p) -> double {
        size_t idx = static_cast<size_t>(p * static_cast<double>(latencies.size() - 1) + 0.5);
        return latencies[idx];
    };

    stats.p50_ms = percentile(0.50);
    stats.p90_ms = percentile(0.90);
    stats.p95_ms = percentile(0.95);
    stats.p99_ms = percentile(0.99);

    return stats;
}

MetricsSnapshot TelemetrySession::aggregate_metrics() const {
    MetricsSnapshot agg;
    for (const auto& s : samples_) {
        agg += s;
    }
    return agg;
}

std::string TelemetrySession::summary() const {
    std::ostringstream ss;
    auto stats = wall_time_stats();
    auto agg = aggregate_metrics();

    ss << "=== Telemetry Session: " << name_ << " (" << stats.sample_count << " samples) ===\n";
    ss << std::fixed << std::setprecision(3);
    ss << "Latency (Wall ms): Mean=" << stats.mean_ms
       << " | Min=" << stats.min_ms
       << " | P50=" << stats.p50_ms
       << " | P90=" << stats.p90_ms
       << " | P95=" << stats.p95_ms
       << " | P99=" << stats.p99_ms
       << " | Max=" << stats.max_ms << "\n";
    ss << "Total Involuntary Switches: " << agg.involuntary_context_switches << "\n";
    ss << "Total Voluntary Switches:   " << agg.voluntary_context_switches << "\n";
    if (agg.hardware_counters_available) {
        ss << "Total Cycles:               " << agg.cycles << "\n";
        ss << "Total Instructions:         " << agg.instructions << "\n";
        ss << "Avg IPC:                    " << std::setprecision(2) << agg.ipc() << "\n";
    }
    return ss.str();
}

std::string TelemetrySession::to_json() const {
    auto stats = wall_time_stats();
    auto agg = aggregate_metrics();

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);
    ss << "{\n";
    std::string escaped;
    for (char ch : name_) {
        if (ch == '"' || ch == '\\') escaped += '_';
        else escaped += ch;
    }
    ss << "  \"name\": \"" << escaped << "\",\n";
    ss << "  \"sample_count\": " << stats.sample_count << ",\n";
    ss << "  \"latency_ms\": {\n";
    ss << "    \"min\": " << stats.min_ms << ",\n";
    ss << "    \"max\": " << stats.max_ms << ",\n";
    ss << "    \"mean\": " << stats.mean_ms << ",\n";
    ss << "    \"p50\": " << stats.p50_ms << ",\n";
    ss << "    \"p90\": " << stats.p90_ms << ",\n";
    ss << "    \"p95\": " << stats.p95_ms << ",\n";
    ss << "    \"p99\": " << stats.p99_ms << "\n";
    ss << "  },\n";
    ss << "  \"aggregate\": " << agg.to_json() << "\n";
    ss << "}";
    return ss.str();
}

} // namespace affix
