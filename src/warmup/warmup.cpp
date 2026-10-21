#include "affix/warmup.hpp"
#include <chrono>

namespace affix {

void warmup_core(std::chrono::nanoseconds duration) {
    auto start = std::chrono::steady_clock::now();
    volatile uint64_t counter = 0;
    while (std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start) < duration) {
        counter = counter * 1664525ULL + 1013904223ULL;
    }
}

void spin_pause(size_t iterations) {
    for (size_t i = 0; i < iterations; ++i) {
#if defined(__x86_64__) || defined(_M_X64)
        asm volatile("pause" ::: "memory");
#elif defined(__aarch64__) || defined(_M_ARM64)
        asm volatile("yield" ::: "memory");
#else
        std::this_thread::yield();
#endif
    }
}

} // namespace affix
