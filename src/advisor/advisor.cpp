#include "affix/advisor.hpp"
#include <algorithm>

namespace affix {

namespace internal {

AdvisorReport run_evaluations(const std::vector<Policy>& policies,
                              const std::vector<TelemetrySession>& sessions,
                              size_t baseline_index) {
    AdvisorReport report;
    report.baseline_index = baseline_index;
    report.evaluations.reserve(policies.size());

    for (size_t i = 0; i < policies.size(); ++i) {
        PolicyEvaluation eval;
        eval.policy = policies[i];
        eval.latency = sessions[i].wall_time_stats();
        eval.aggregate = sessions[i].aggregate_metrics();
        report.evaluations.push_back(eval);
    }

    const auto& baseline_latency = report.evaluations[baseline_index].latency;
    const auto& baseline_agg = report.evaluations[baseline_index].aggregate;

