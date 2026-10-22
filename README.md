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

