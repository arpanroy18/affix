#include "affix/affix.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <cmath>

void run_topology_cmd(bool json) {
    const auto& topo = affix::get_system_topology();
    if (json) {
        std::cout << topo.to_json() << std::endl;
    } else {
        std::cout << topo.summary() << std::endl;
    }
}

void run_benchmark_cmd(uint32_t target_core, size_t iterations) {
    std::cout << "Running Affix Benchmark (" << iterations << " iterations per policy)...\n\n";

    auto compute_work = []() {
        volatile double val = 1.0;
        for (int i = 0; i < 500'000; ++i) {
            val = std::sin(val) * std::cos(val) + 1.0;
        }
    };

    // 1. Unpinned execution
    affix::Workload unpinned("Unpinned (OS default)", affix::Policy::unrestricted());
    for (size_t i = 0; i < iterations; ++i) {
        unpinned.run(compute_work);
    }

    // 2. Pinned execution
    affix::Workload pinned("Pinned (Core " + std::to_string(target_core) + ")",
                           affix::Policy::pinned(target_core));
    for (size_t i = 0; i < iterations; ++i) {
        pinned.run(compute_work);
    }

    std::cout << unpinned.session().summary() << "\n";
    std::cout << pinned.session().summary() << "\n";

    auto unpinned_stats = unpinned.session().wall_time_stats();
    auto pinned_stats = pinned.session().wall_time_stats();

    std::cout << "=== Comparison Summary ===\n";
    std::cout << "P50 Latency:  Unpinned = " << unpinned_stats.p50_ms << " ms | Pinned = " << pinned_stats.p50_ms << " ms\n";
    std::cout << "P99 Latency:  Unpinned = " << unpinned_stats.p99_ms << " ms | Pinned = " << pinned_stats.p99_ms << " ms\n";
    if (unpinned_stats.p99_ms > 0) {
        double jitter_diff = ((pinned_stats.p99_ms - unpinned_stats.p99_ms) / unpinned_stats.p99_ms) * 100.0;
        std::cout << "P99 Jitter delta: " << (jitter_diff > 0 ? "+" : "") << jitter_diff << " %\n";
    }
}

void run_advisor_cmd(size_t iterations, bool json) {
    const auto& topo = affix::get_system_topology();
    std::vector<affix::Policy> policies = {
        affix::Policy::unrestricted(),
        affix::Policy::pinned(0),
        affix::Policy::physical_core(0),
        affix::Policy::performance_cores()
    };

    if (topo.has_isolated_cpus()) {
        policies.push_back(affix::Policy::isolated_core());
    }

    auto compute_work = []() {
        volatile double val = 1.0;
        for (int i = 0; i < 200'000; ++i) {
            val = std::sin(val) * std::cos(val) + 1.0;
        }
    };

    auto report = affix::evaluate_placement(compute_work, policies, iterations, 5);
    if (json) {
        std::cout << report.to_json() << std::endl;
    } else {
        std::cout << report.summary() << std::endl;
    }
}

