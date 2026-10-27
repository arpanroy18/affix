#pragma once

#include "policy.hpp"
#include "placement.hpp"
#include "observability.hpp"
#include <string>
#include <functional>
#include <utility>

namespace affix {

class Workload {
public:
    explicit Workload(std::string name, Policy policy = Policy::unrestricted())
        : name_(std::move(name)), policy_(std::move(policy)), session_(name_) {}

    void set_placement(const Policy& policy) {
        policy_ = policy;
    }

    [[nodiscard]] const Policy& policy() const { return policy_; }
    [[nodiscard]] const std::string& name() const { return name_; }
    [[nodiscard]] const MetricsSnapshot& last_metrics() const { return last_metrics_; }
    [[nodiscard]] const TelemetrySession& session() const { return session_; }

    template <typename Func>
    auto run(Func&& func) {
        ScopedPlacement placement_guard(policy_);
        MetricsSnapshot delta;
        if constexpr (std::is_void_v<std::invoke_result_t<Func>>) {
            {
                ScopedProbe probe(&delta);
                func();
            }
            last_metrics_ = delta;
            session_.record(delta);
        } else {
            auto execute = [&]() {
                ScopedProbe probe(&delta);
                return func();
            };
            auto result = execute();
            last_metrics_ = delta;
            session_.record(delta);
            return result;
        }
    }

private:
    std::string name_;
    Policy policy_;
    MetricsSnapshot last_metrics_;
    TelemetrySession session_;
};

} // namespace affix
