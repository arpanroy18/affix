#include "affix/placement.hpp"
#include "affix/topology.hpp"
#include "../platform/platform.hpp"

namespace affix {

namespace {

PlacementResult do_apply_affinity(pthread_t handle, const Policy& policy) {
    PlacementResult res;
    const auto& topo = SystemTopology::instance();
    res.requested_mask = policy.resolve(topo);

    if (res.requested_mask.empty()) {
        res.applied = false;
        res.error_message = "Resolved CPU mask is empty or not allowed by system policy";
        return res;
    }

    res.applied = internal::platform_set_thread_affinity(handle, res.requested_mask, &res.effective_mask, &res.error_message);

    if (policy.sched_priority() != SchedPriority::Default) {
        res.priority_applied = internal::platform_set_thread_priority(
            handle, policy.sched_priority(), policy.priority_value(), &res.priority_message);
    }
    return res;
}

} // namespace

