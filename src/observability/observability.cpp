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

