#include "corepilot/corepilot.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <cmath>

void matrix_multiply(size_t n, std::vector<double>& a, std::vector<double>& b, std::vector<double>& c) {
    for (size_t i = 0; i < n; ++i) {
        for (size_t k = 0; k < n; ++k) {
            for (size_t j = 0; j < n; ++j) {
                c[i * n + j] += a[i * n + k] * b[k * n + j];
            }
        }
    }
}

int main(int argc, char* argv[]) {
    size_t iterations = 30;
    if (argc >= 2) {
        iterations = std::stoul(argv[1]);
    }

    const auto& topo = corepilot::get_system_topology();
    std::cout << "CorePilot Contention & Placement Benchmark\n";
    std::cout << "System: " << topo.logical_core_count() << " logical CPUs, "
              << topo.physical_core_count() << " physical cores\n\n";

    const size_t N = 120;
    std::vector<double> a(N * N, 1.01);
    std::vector<double> b(N * N, 0.99);
    std::vector<double> c(N * N, 0.0);

    auto work = [&]() {
        matrix_multiply(N, a, b, c);
    };

    // 1. Clean run - Unpinned
    std::cout << "--> Running Scenario 1: Clean Unpinned Workload (" << iterations << " iterations)...\n";
    corepilot::Workload unpinned("Clean_Unpinned", corepilot::Policy::unrestricted());
    for (size_t i = 0; i < iterations; ++i) {
        unpinned.run(work);
    }
    std::cout << unpinned.session().summary() << "\n";

    // 2. Clean run - Pinned
    uint32_t target_core = 0;
    std::cout << "--> Running Scenario 2: Clean Pinned Workload (Core " << target_core << ", " << iterations << " iterations)...\n";
    corepilot::Workload pinned("Clean_Pinned", corepilot::Policy::pinned(target_core));
    for (size_t i = 0; i < iterations; ++i) {
        pinned.run(work);
    }
    std::cout << pinned.session().summary() << "\n";

    // 3. Contention run - With background noisy neighbor threads
    std::cout << "--> Spawning background noisy neighbor threads...\n";
    std::atomic<bool> stop_noisy{false};
    std::vector<std::thread> noisy_threads;
    // Spawn 2 noisy neighbor threads that compete for CPU
    for (int t = 0; t < 2; ++t) {
        noisy_threads.emplace_back([&stop_noisy]() {
            volatile double x = 1.0;
            while (!stop_noisy.load(std::memory_order_relaxed)) {
                x = std::sin(x) + std::cos(x);
            }
        });
    }

    // A: Unpinned under contention
    std::cout << "--> Running Scenario 3A: Unpinned Under Contention...\n";
    corepilot::Workload unpinned_contended("Contended_Unpinned", corepilot::Policy::unrestricted());
    for (size_t i = 0; i < iterations; ++i) {
        unpinned_contended.run(work);
    }

    // B: Pinned to isolated core under contention
    // Pin to core 1 while noisy neighbors run unpinned or on other cores
    uint32_t isolated_core = (topo.logical_core_count() > 1) ? 1 : 0;
    std::cout << "--> Running Scenario 3B: Pinned to Isolated Core " << isolated_core << " Under Contention...\n";
    corepilot::Workload pinned_isolated("Contended_Pinned_Isolated", corepilot::Policy::pinned(isolated_core));
    for (size_t i = 0; i < iterations; ++i) {
        pinned_isolated.run(work);
    }

    stop_noisy.store(true);
    for (auto& t : noisy_threads) {
        t.join();
    }

    std::cout << unpinned_contended.session().summary() << "\n";
    std::cout << pinned_isolated.session().summary() << "\n";

    auto stats_unpinned = unpinned_contended.session().wall_time_stats();
    auto stats_pinned = pinned_isolated.session().wall_time_stats();

    std::cout << "============================================================\n";
    std::cout << "FINAL OBSERVABILITY VERDICT UNDER CONTENTION\n";
    std::cout << "============================================================\n";
    std::cout << "Unpinned P99 Latency:  " << stats_unpinned.p99_ms << " ms\n";
    std::cout << "Pinned P99 Latency:    " << stats_pinned.p99_ms << " ms\n";
    if (stats_unpinned.p99_ms > 0) {
        double diff = ((stats_unpinned.p99_ms - stats_pinned.p99_ms) / stats_unpinned.p99_ms) * 100.0;
        std::cout << "Latency Reduction:     " << diff << " %\n";
    }
    std::cout << "============================================================\n";

    return 0;
}
