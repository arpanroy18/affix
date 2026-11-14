#pragma once

#include "corepilot/types.hpp"
#include "corepilot/topology.hpp"
#include "corepilot/observability.hpp"
#include <pthread.h>

namespace corepilot::internal {

bool platform_set_thread_affinity(pthread_t handle, const CpuSet& mask, CpuSet* out_effective, std::string* error_msg);
bool platform_get_thread_affinity(pthread_t handle, CpuSet& mask, std::string* error_msg);

bool platform_set_current_thread_affinity(const CpuSet& mask, CpuSet* out_effective, std::string* error_msg);
bool platform_get_current_thread_affinity(CpuSet& mask, std::string* error_msg);

bool platform_set_tid_affinity(uint64_t tid, const CpuSet& mask, CpuSet* out_effective, std::string* error_msg);

uint32_t platform_get_current_cpu();
uint64_t platform_get_current_tid();

void platform_discover_topology(SystemTopology& topo);

MetricsSnapshot platform_capture_thread_metrics();

} // namespace corepilot::internal
