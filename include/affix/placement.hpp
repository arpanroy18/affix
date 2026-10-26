#pragma once

#include "types.hpp"
#include "policy.hpp"
#include <string>
#include <thread>

namespace affix {

struct PlacementResult {
    bool applied = false;
    bool priority_applied = false;
    CpuSet requested_mask;
    CpuSet effective_mask;
    std::string error_message;
    std::string priority_message;

    [[nodiscard]] explicit operator bool() const { return applied; }
};

PlacementResult apply_placement(std::thread& thread, const Policy& policy);
PlacementResult apply_placement(std::jthread& thread, const Policy& policy);
PlacementResult apply_placement(pthread_t native_handle, const Policy& policy);
PlacementResult apply_placement_to_thread_id(uint64_t tid, const Policy& policy);

class ScopedPlacement {
public:
    explicit ScopedPlacement(const Policy& policy);
    ~ScopedPlacement();

    ScopedPlacement(const ScopedPlacement&) = delete;
    ScopedPlacement& operator=(const ScopedPlacement&) = delete;
    ScopedPlacement(ScopedPlacement&& other) noexcept;
    ScopedPlacement& operator=(ScopedPlacement&& other) noexcept;

    [[nodiscard]] const PlacementResult& result() const { return result_; }
    [[nodiscard]] bool applied() const { return result_.applied; }

private:
    PlacementResult result_;
    CpuSet previous_mask_;
    SchedPriority previous_sched_ = SchedPriority::Default;
    int previous_priority_val_ = 0;
    bool active_ = false;
};

} // namespace affix
