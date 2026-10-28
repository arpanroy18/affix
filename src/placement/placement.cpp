#include "affix/placement.hpp"
#include "affix/topology.hpp"
#include "../platform/platform.hpp"
#include <utility>

namespace affix {

namespace {

void restore_thread_state(bool active, const CpuSet& mask, SchedPriority sched, int sched_val) {
    if (!active) return;
    if (!mask.empty()) {
        internal::platform_set_current_thread_affinity(mask, nullptr, nullptr);
    }
    if (sched != SchedPriority::Default) {
        internal::platform_set_current_thread_priority(sched, sched_val, nullptr);
    }
}

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

PlacementResult apply_placement(std::thread& thread, const Policy& policy) {
    return do_apply_affinity(thread.native_handle(), policy);
}

PlacementResult apply_placement(std::jthread& thread, const Policy& policy) {
    return do_apply_affinity(thread.native_handle(), policy);
}

PlacementResult apply_placement(pthread_t native_handle, const Policy& policy) {
    return do_apply_affinity(native_handle, policy);
}

PlacementResult apply_placement_to_thread_id(uint64_t tid, const Policy& policy) {
    PlacementResult res;
    const auto& topo = SystemTopology::instance();
    res.requested_mask = policy.resolve(topo);

    if (res.requested_mask.empty()) {
        res.applied = false;
        res.error_message = "Resolved CPU mask is empty";
        return res;
    }

    res.applied = internal::platform_set_tid_affinity(tid, res.requested_mask, &res.effective_mask, &res.error_message);
    return res;
}

ScopedPlacement::ScopedPlacement(const Policy& policy) {
    internal::platform_get_current_thread_affinity(previous_mask_, nullptr);
    internal::platform_get_current_thread_priority(previous_sched_, previous_priority_val_, nullptr);

    const auto& topo = SystemTopology::instance();
    result_.requested_mask = policy.resolve(topo);

    if (!result_.requested_mask.empty()) {
        result_.applied = internal::platform_set_current_thread_affinity(
            result_.requested_mask, &result_.effective_mask, &result_.error_message);
        active_ = result_.applied;

        if (policy.sched_priority() != SchedPriority::Default) {
            result_.priority_applied = internal::platform_set_current_thread_priority(
                policy.sched_priority(), policy.priority_value(), &result_.priority_message);
        }
    } else {
        result_.applied = false;
        result_.error_message = "Resolved CPU mask is empty";
    }
}

ScopedPlacement::~ScopedPlacement() {
    restore_thread_state(active_, previous_mask_, previous_sched_, previous_priority_val_);
}

ScopedPlacement::ScopedPlacement(ScopedPlacement&& other) noexcept
    : result_(std::move(other.result_)),
      previous_mask_(std::move(other.previous_mask_)),
      previous_sched_(other.previous_sched_),
      previous_priority_val_(other.previous_priority_val_),
      active_(other.active_) {
    other.active_ = false;
}

ScopedPlacement& ScopedPlacement::operator=(ScopedPlacement&& other) noexcept {
    if (this != &other) {
        restore_thread_state(active_, previous_mask_, previous_sched_, previous_priority_val_);
        result_ = std::move(other.result_);
        previous_mask_ = std::move(other.previous_mask_);
        previous_sched_ = other.previous_sched_;
        previous_priority_val_ = other.previous_priority_val_;
        active_ = other.active_;
        other.active_ = false;
    }
    return *this;
}

} // namespace affix
