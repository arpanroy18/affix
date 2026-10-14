#pragma once

#include "types.hpp"
#include "policy.hpp"
#include "placement.hpp"
#include "warmup.hpp"
#include <chrono>

namespace affix::this_thread {

PlacementResult set_placement(const Policy& policy);
CpuSet get_placement();
uint32_t get_current_core();
uint64_t get_id();

bool set_priority(SchedPriority priority, int val = 0, std::string* error_msg = nullptr);
bool get_priority(SchedPriority& priority, int& val, std::string* error_msg = nullptr);

inline void warmup(std::chrono::nanoseconds duration = std::chrono::milliseconds(5)) {
    ::affix::warmup_core(duration);
}

} // namespace affix::this_thread
