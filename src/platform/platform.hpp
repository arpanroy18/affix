#pragma once

#include "affix/types.hpp"
#include "affix/topology.hpp"
#include "affix/policy.hpp"
#include "affix/observability.hpp"
#include <pthread.h>

namespace affix::internal {

bool platform_set_thread_affinity(pthread_t handle, const CpuSet& mask, CpuSet* out_effective, std::string* error_msg);
bool platform_get_thread_affinity(pthread_t handle, CpuSet& mask, std::string* error_msg);

bool platform_set_current_thread_affinity(const CpuSet& mask, CpuSet* out_effective, std::string* error_msg);
bool platform_get_current_thread_affinity(CpuSet& mask, std::string* error_msg);

bool platform_set_tid_affinity(uint64_t tid, const CpuSet& mask, CpuSet* out_effective, std::string* error_msg);

bool platform_set_thread_priority(pthread_t handle, SchedPriority priority, int val, std::string* error_msg);
bool platform_get_thread_priority(pthread_t handle, SchedPriority& priority, int& val, std::string* error_msg);

bool platform_set_current_thread_priority(SchedPriority priority, int val, std::string* error_msg);
bool platform_get_current_thread_priority(SchedPriority& priority, int& val, std::string* error_msg);

uint32_t platform_get_current_cpu();
uint64_t platform_get_current_tid();

void platform_discover_topology(SystemTopology& topo);

MetricsSnapshot platform_capture_thread_metrics();

bool platform_bind_memory_to_numa(void* ptr, size_t size, NumaId node, std::string* error_msg);
bool platform_set_thread_preferred_numa(NumaId node, std::string* error_msg);
void* platform_allocate_numa(size_t size, NumaId node);
void platform_free_numa(void* ptr, size_t size);

} // namespace affix::internal
