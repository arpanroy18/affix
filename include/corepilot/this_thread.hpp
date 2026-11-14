#pragma once

#include "types.hpp"
#include "policy.hpp"
#include "placement.hpp"

namespace corepilot::this_thread {

PlacementResult set_placement(const Policy& policy);
CpuSet get_placement();
uint32_t get_current_core();
uint64_t get_id();

} // namespace corepilot::this_thread
