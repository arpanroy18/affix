# CorePilot

> **Explicit CPU placement. Measurable performance.**

CorePilot is a lightweight, zero-dependency C++20 library for **CPU core assignment, thread placement, workload isolation, and performance observability**.

Instead of manually wrestling with OS-specific affinity masks (`sched_setaffinity`, `pthread_setaffinity_np`, or Mach policies), CorePilot provides a modern, clean abstraction to assign threads to cores and directly **measure whether placement actually improved throughput, tail latency, and jitter**.

---

## Key Features

- **Hardware Topology Discovery:** Enumerates logical CPUs, physical cores, SMT siblings, NUMA nodes, cache hierarchies, and distinguishes Performance (P) vs. Efficient (E) cores.
- **Drop-in Retrofit for Existing Codebases:** No need to rewrite existing thread pools. Pin calling threads in-situ, pin existing `std::thread` / `pthread_t` handles, or wrap critical sections with RAII guards.
- **Three-Tiered Observability:**
  - **Tier 0:** High-precision wall-clock time and thread CPU time.
  - **Tier 1:** Voluntary and involuntary OS context switches (`getrusage`) and CPU core migration detection.
  - **Tier 2:** Hardware PMU counters (Cycles, Instructions, IPC, L1 and LLC cache misses via `perf_event_open`) with graceful, non-crashing permission fallback.
- **Zero Mandatory Dependencies:** Written in clean modern C++20; communicates directly with Linux sysfs/kernel APIs and Darwin sysctl/Mach interfaces.
- **Built-in CLI & Benchmark Suite:** Inspect CPU topologies in human or JSON formats, and benchmark pinned vs. unpinned latency distributions (P50, P90, P99).

---

## Integration into Existing Projects

### 1. CMake Integration (`FetchContent`)

Add CorePilot to your project's `CMakeLists.txt` in seconds:

```cmake
include(FetchContent)
FetchContent_Declare(
    corepilot
    GIT_REPOSITORY https://github.com/your-org/corepilot.git
    GIT_TAG        v0.1.0
)
FetchContent_MakeAvailable(corepilot)

target_link_libraries(my_existing_app PRIVATE corepilot::corepilot)
```

---

## Usage Examples

### 1. In-Situ Self-Pinning (1-Liner inside an existing worker loop)

If you already have a worker function or thread loop, pin it at the top of the function:

```cpp
#include <corepilot/corepilot.hpp>

void worker_thread_loop() {
    // Pin this thread to logical CPU 2
    corepilot::this_thread::set_placement(corepilot::Policy::pinned(2));

    while (running) {
        process_incoming_data(); // Business logic remains untouched
    }
}
```

### 2. Pinning Existing Thread Handles

If you manage a thread pool or spawn threads via `std::thread`:

```cpp
#include <corepilot/corepilot.hpp>
#include <thread>

std::thread worker(&do_work);

// Pin the existing thread to physical core 0 (including its SMT siblings)
corepilot::apply_placement(worker, corepilot::Policy::physical_core(0));

worker.join();
```

### 3. RAII Scoped Placement & Telemetry Probing

Temporarily pin a critical calculation and record latency, CPU time, and context switches:

```cpp
#include <corepilot/corepilot.hpp>

void compute_critical_path() {
    // Pin to core 1 for this scope only (previous affinity restored on exit)
    corepilot::ScopedPlacement placement(corepilot::Policy::pinned(1));

    corepilot::MetricsSnapshot metrics;
    {
        // Measure execution metrics across this block
        corepilot::ScopedProbe probe(&metrics);
        heavy_math_routine();
    }

    std::cout << "P99 Wall Time: " << (metrics.wall_time_ns / 1'000'000.0) << " ms\n";
    std::cout << "Involuntary Preemptions: " << metrics.involuntary_context_switches << "\n";
}
```

### 4. Managed Workload Engine

For end-to-end task runs with automatic telemetry aggregation:

```cpp
#include <corepilot/corepilot.hpp>

int main() {
    corepilot::Workload backtest("HFT_Backtest", corepilot::Policy::pinned(3));

    // Run batch iterations
    for (int i = 0; i < 100; ++i) {
        backtest.run([&]() {
            simulate_market_tick();
        });
    }

    // Print summary with mean, P50, P90, P95, and P99 latency percentiles
    std::cout << backtest.session().summary() << "\n";
    return 0;
}
```

---

## Policies Overview

| Policy | Behavior |
| :--- | :--- |
| `Policy::pinned(core_id)` | Restricts execution to a specific logical CPU. |
| `Policy::physical_core(id)` | Restricts execution to a physical core and all its SMT siblings. |
| `Policy::cpu_set({1, 2, 4})` | Restricts execution to an explicit set of CPUs. |
| `Policy::numa_node(node_id)` | Restricts execution to all CPUs associated with a NUMA node. |
| `Policy::performance_cores()`| Binds to discovered high-performance (P) cores. |
| `Policy::efficient_cores()`  | Binds to discovered efficiency (E) cores. |
| `Policy::preferred(core_id)` | Soft preference hint without hard restriction. |
| `Policy::unrestricted()`     | Default OS scheduler (any allowed CPU). |

---

## CLI Tool & Benchmark

### Build

```bash
mkdir build && cd build
cmake ..
cmake --build . -j
```

### Inspect Hardware Topology

```bash
./corepilot-cli topology
```
Output:
```
=== CPU Topology Summary ===
Logical Processors:  8
Physical Cores:      8
NUMA Nodes:          1
Process Allowed CPUs:0-7

CPU     Physical    Package   NUMA    Type              SMT Siblings
--------------------------------------------------------------------
0       0           0         0       Performance (P-core)0
1       1           0         0       Performance (P-core)1
2       2           0         0       Performance (P-core)2
3       3           0         0       Performance (P-core)3
4       4           0         0       Performance (P-core)4
5       5           0         0       Performance (P-core)5
6       6           0         0       Efficient (E-core)6
7       7           0         0       Efficient (E-core)7
```

JSON output is also supported:
```bash
./corepilot-cli topology --json
```

### Run Contention Benchmark

```bash
./corepilot-bench 20
```

Demonstrates the latency degradation of unpinned threads when competing with noisy neighbor processes versus the deterministic latency of a core-pinned thread.

---

## Running Unit Tests

```bash
cd build
ctest --output-on-failure
```

---

## License

MIT License.
