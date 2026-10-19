#include "affix/this_thread.hpp"
#include "affix/topology.hpp"
#include "../platform/platform.hpp"

namespace affix::this_thread {

PlacementResult set_placement(const Policy& policy) {
    PlacementResult res;
    const auto& topo = SystemTopology::instance();
    res.requested_mask = policy.resolve(topo);

    if (res.requested_mask.empty()) {
        res.applied = false;
        res.error_message = "Resolved CPU mask is empty";
        return res;
    }

    res.applied = internal::platform_set_current_thread_affinity(
        res.requested_mask, &res.effective_mask, &res.error_message);
    return res;
}

CpuSet get_placement() {
    CpuSet mask;
    internal::platform_get_current_thread_affinity(mask, nullptr);
    return mask;
}

uint32_t get_current_core() {
    return internal::platform_get_current_cpu();
}

uint64_t get_id() {
    return internal::platform_get_current_tid();
}

bool set_priority(SchedPriority priority, int val, std::string* error_msg) {
    return internal::platform_set_current_thread_priority(priority, val, error_msg);
}

bool get_priority(SchedPriority& priority, int& val, std::string* error_msg) {
    return internal::platform_get_current_thread_priority(priority, val, error_msg);
}

} // namespace affix::this_thread
