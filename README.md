# Affix

> **Explicit CPU placement. Measurable performance.**

Affix is a lightweight, zero-dependency C++20 library for **CPU core assignment, thread placement, workload isolation, and performance observability**.

Instead of manually wrestling with OS-specific affinity masks (`sched_setaffinity`, `pthread_setaffinity_np`, or Mach policies), Affix provides a modern, clean abstraction to assign threads to cores, isolate critical workloads, and directly **measure whether placement actually improved throughput, tail latency, and jitter**.

---

## Key Features

- **Hardware Topology Discovery:** Enumerates logical CPUs, physical cores, SMT siblings, NUMA nodes, cache hierarchies, kernel-isolated cores (`isolcpus`), and distinguishes Performance (P) vs. Efficient (E) cores.
- **Real-Time Priority Scheduling:** Combine CPU affinity with `SCHED_FIFO` or `SCHED_RR` priorities to eliminate kernel preemption.
- **Automated Placement Advisor:** A/B trial harness that programmatically tests placement options and recommends the optimal policy for tail latency and context switch reduction.
- **NUMA-Aware Memory Binding & STL Allocator:** Bind memory allocations to local NUMA nodes (`affix::numa::NumaAllocator<T>`) to eliminate cross-socket memory bus penalties.
- **Core Warm-Up & DVFS Stabilizer:** Ramps idle cores out of deep sleep states (C-states) into maximum clock frequency before critical executions to prevent false latency jitter.
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

Add Affix to your project's `CMakeLists.txt` in seconds:

```cmake
include(FetchContent)
FetchContent_Declare(
    affix
    GIT_REPOSITORY https://github.com/your-org/affix.git
    GIT_TAG        v0.2.0
)
FetchContent_MakeAvailable(affix)

target_link_libraries(my_existing_app PRIVATE affix::affix)
```

---

## Usage Examples

### 1. In-Situ Self-Pinning (1-Liner inside an existing worker loop)

If you already have a worker function or thread loop, pin it at the top of the function:

```cpp
#include <affix/affix.hpp>

void worker_thread_loop() {
    // Pin this thread to logical CPU 2
    affix::this_thread::set_placement(affix::Policy::pinned(2));

    while (running) {
        process_incoming_data(); // Business logic remains untouched
    }
}
```

### 2. Pinning Existing Thread Handles

If you manage a thread pool or spawn threads via `std::thread`:

```cpp
#include <affix/affix.hpp>
#include <thread>

std::thread worker(&do_work);

// Pin the existing thread to physical core 0 (including its SMT siblings)
affix::apply_placement(worker, affix::Policy::physical_core(0));

worker.join();
```

### 3. RAII Scoped Placement & Telemetry Probing

Temporarily pin a critical calculation and record latency, CPU time, and context switches:

```cpp
#include <affix/affix.hpp>

void compute_critical_path() {
    // Pin to core 1 for this scope only (previous affinity restored on exit)
    affix::ScopedPlacement placement(affix::Policy::pinned(1));

    affix::MetricsSnapshot metrics;
    {
        // Measure execution metrics across this block
        affix::ScopedProbe probe(&metrics);
        heavy_math_routine();
    }

    std::cout << "P99 Wall Time: " << (metrics.wall_time_ns / 1'000'000.0) << " ms\n";
    std::cout << "Involuntary Preemptions: " << metrics.involuntary_context_switches << "\n";
}
```

### 4. Managed Workload Engine

For end-to-end task runs with automatic telemetry aggregation:

```cpp
#include <affix/affix.hpp>

int main() {
    affix::Workload backtest("HFT_Backtest", affix::Policy::pinned(3));

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

### 5. Automated Placement Advisor (A/B Validator)

Automatically trial placement options on a workload and get a data-driven recommendation:

```cpp
#include <affix/affix.hpp>

auto report = affix::evaluate_placement(my_function, {
    affix::Policy::unrestricted(),
    affix::Policy::pinned(0),
    affix::Policy::physical_core(0),
    affix::Policy::performance_cores()
}, /*iterations=*/50, /*warmup=*/5);

std::cout << report.summary() << "\n";
```

### 6. NUMA Memory Binding & Local Allocator

Avoid cross-socket memory bus penalties by keeping thread data strictly on the local NUMA node:

```cpp
#include <affix/affix.hpp>
#include <vector>

// STL vector allocated on NUMA Node 0
std::vector<Order, affix::numa::NumaAllocator<Order>> order_book(
    affix::numa::NumaAllocator<Order>(/*numa_node=*/0)
);

// Or bind existing raw memory block:
affix::numa::bind_memory(raw_buffer, buffer_size, /*numa_node=*/0);
```

### 7. Real-Time Scheduling Priority Elevation

Pair core affinity with real-time FIFO scheduling to prevent the OS kernel from preempting the thread:

```cpp
#include <affix/affix.hpp>

// Pin to core 2 with maximum real-time FIFO priority
auto rt_policy = affix::Policy::pinned(2).with_realtime_fifo(99);
affix::this_thread::set_placement(rt_policy);
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
| `Policy::isolated_core()`    | Binds to kernel-isolated CPU (`isolcpus`), falling back to P-core. |
| `Policy::preferred(core_id)` | Soft preference hint without hard restriction. |
| `Policy::unrestricted()`     | Default OS scheduler (any allowed CPU). |
| `.with_realtime_fifo(prio)`  | Elevates scheduling priority to `SCHED_FIFO` (1-99). |
| `.with_realtime_rr(prio)`    | Elevates scheduling priority to `SCHED_RR` (1-99). |

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
./affix-cli topology
```
Output:
```
=== CPU Topology Summary ===
Logical Processors:  8
Physical Cores:      8
NUMA Nodes:          1
Process Allowed CPUs:0-7
Kernel Isolated CPUs:None

CPU     Physical    Package   NUMA    Type              Isolated    SMT Siblings
--------------------------------------------------------------------------------
0       0           0         0       Performance (P-core)No          0
1       1           0         0       Performance (P-core)No          1
2       2           0         0       Performance (P-core)No          2
3       3           0         0       Performance (P-core)No          3
4       4           0         0       Performance (P-core)No          4
5       5           0         0       Performance (P-core)No          5
6       6           0         0       Efficient (E-core)No          6
7       7           0         0       Efficient (E-core)No          7
```

JSON output is also supported:
```bash
./affix-cli topology --json
```

### Run Placement Advisor

```bash
./affix-cli advise --iters 30
```

### Run Contention Benchmark

```bash
./affix-bench 20
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

---

## Benchmarks

`affix-bench` runs the same reduction kernel unpinned, pinned, and pinned to an isolated core while noisy-neighbor threads compete for the machine, then reports P50/P90/P99 wall latency from `TelemetrySession`.

`affix-cli advise` automates the same A/B trial for the standard policies and prints the winning placement policy as text or JSON.

---

## Quick Reference

| Command | Purpose |
| --- | --- |
| `affix-cli topology [--json]` | Inspect logical CPUs, P/E cores, NUMA nodes |
| `affix-cli bench --core N --iters M` | Benchmark unpinned vs pinned latency |
| `affix-cli advise --iters M [--json]` | Run the automated placement advisor |

## Guard Rails

Wrap a critical section in `affix::ScopedPlacement` (RAII) to pin the calling thread for the duration of the block and automatically restore the previous affinity on scope exit.
