#pragma once

#include "policy.hpp"
#include "placement.hpp"
#include "observability.hpp"
#include "workload.hpp"
#include "warmup.hpp"
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

namespace affix {

struct PolicyEvaluation {
    Policy policy;
    LatencyStats latency;
    MetricsSnapshot aggregate;
    double p99_improvement_pct = 0.0;
    int64_t context_switch_reduction = 0;
};

struct AdvisorReport {
    std::vector<PolicyEvaluation> evaluations;
    size_t baseline_index = 0;
    size_t best_index = 0;
    std::string recommendation;

    std::string summary() const;
    std::string to_json() const;
};

namespace internal {

[[nodiscard]] AdvisorReport run_evaluations(const std::vector<Policy>& policies,
                              const std::vector<TelemetrySession>& sessions,
                              size_t baseline_index);

} // namespace internal

template <typename Func>
AdvisorReport evaluate_placement(Func&& func,
                                 const std::vector<Policy>& policies,
                                 size_t iterations = 30,
                                 size_t warmup_iterations = 5) {
    std::vector<TelemetrySession> sessions;
    sessions.reserve(policies.size());

    for (size_t p = 0; p < policies.size(); ++p) {
        const auto& policy = policies[p];
        std::string name = "Policy_" + std::to_string(p) + "_" + policy.to_string();
        Workload wl(name, policy);

        // Core warm-up
        warmup_core(std::chrono::milliseconds(2));

        // Warmup runs
        for (size_t w = 0; w < warmup_iterations; ++w) {
            wl.run(func);
        }

        // Fresh session for measured iterations
        TelemetrySession session(policy.to_string());
        for (size_t i = 0; i < iterations; ++i) {
            wl.run(func);
            session.record(wl.last_metrics());
        }
        sessions.push_back(session);
    }

    return internal::run_evaluations(policies, sessions, 0);
}

} // namespace affix
