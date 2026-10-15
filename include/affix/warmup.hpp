#pragma once

#include <chrono>
#include <cstddef>

namespace affix {

void warmup_core(std::chrono::nanoseconds duration = std::chrono::milliseconds(5));
void spin_pause(size_t iterations = 100);

} // namespace affix
