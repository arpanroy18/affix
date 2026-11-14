#include "corepilot/corepilot.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <cmath>

void run_topology_cmd(bool json) {
    const auto& topo = corepilot::get_system_topology();
    if (json) {
        std::cout << topo.to_json() << std::endl;
    } else {
        std::cout << topo.summary() << std::endl;
    }
}

void run_benchmark_cmd(uint32_t target_core, size_t iterations) {
    std::cout << "Running CorePilot Benchmark (" << iterations << " iterations per policy)...\n\n";

    auto compute_work = []() {
        volatile double val = 1.0;
        for (int i = 0; i < 500'000; ++i) {
            val = std::sin(val) * std::cos(val) + 1.0;
        }
    };

    // 1. Unpinned execution
    corepilot::Workload unpinned("Unpinned (OS default)", corepilot::Policy::unrestricted());
    for (size_t i = 0; i < iterations; ++i) {
        unpinned.run(compute_work);
    }

    // 2. Pinned execution
    corepilot::Workload pinned("Pinned (Core " + std::to_string(target_core) + ")",
                               corepilot::Policy::pinned(target_core));
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

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "CorePilot CLI v" << corepilot::version() << "\n";
        std::cout << "Usage:\n";
        std::cout << "  corepilot-cli topology [--json]       Inspect CPU topology, NUMA, and caches\n";
        std::cout << "  corepilot-cli bench [--core N]       Benchmark unpinned vs pinned latency\n";
        return 0;
    }

    std::string cmd = argv[1];
    if (cmd == "topology") {
        bool json = (argc >= 3 && std::string(argv[2]) == "--json");
        run_topology_cmd(json);
    } else if (cmd == "bench") {
        uint32_t core = 0;
        size_t iters = 50;
        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--core" && i + 1 < argc) {
                core = static_cast<uint32_t>(std::stoul(argv[++i]));
            } else if (arg == "--iters" && i + 1 < argc) {
                iters = std::stoul(argv[++i]);
            }
        }
        run_benchmark_cmd(core, iters);
    } else {
        std::cerr << "Unknown command: " << cmd << "\n";
        return 1;
    }

    return 0;
}
