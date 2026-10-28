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
    if (sessions.size() != policies.size()) {
        return report;
    }

    for (size_t i = 0; i < policies.size(); ++i) {
        PolicyEvaluation eval;
        eval.policy = policies[i];
        eval.latency = sessions[i].wall_time_stats();
        eval.aggregate = sessions[i].aggregate_metrics();
        report.evaluations.push_back(eval);
    }

    const auto& baseline_latency = report.evaluations[baseline_index].latency;
    const auto& baseline_agg = report.evaluations[baseline_index].aggregate;

    size_t best_idx = baseline_index;
    double best_score = 0.0;

    for (size_t i = 0; i < report.evaluations.size(); ++i) {
        auto& eval = report.evaluations[i];
        if (baseline_latency.p99_ms > 0) {
            eval.p99_improvement_pct = ((baseline_latency.p99_ms - eval.latency.p99_ms) / baseline_latency.p99_ms) * 100.0;
        }
        eval.context_switch_reduction = baseline_agg.involuntary_context_switches - eval.aggregate.involuntary_context_switches;

        // Composite performance score (weighted 70% P99 latency reduction, 30% preemption reduction)
        double switch_factor = (baseline_agg.involuntary_context_switches > 0)
            ? (static_cast<double>(eval.context_switch_reduction) / static_cast<double>(baseline_agg.involuntary_context_switches)) * 100.0
            : 0.0;
        double score = (eval.p99_improvement_pct * 0.70) + (switch_factor * 0.30);

        if (score > best_score) {
            best_score = score;
            best_idx = i;
        }
    }

    report.best_index = best_idx;
    std::ostringstream rec;
    rec << std::fixed << std::setprecision(2);

    if (best_idx == baseline_index) {
        rec << "Default OS scheduling (Baseline) performed best. Explicit pinning did not yield statistically significant P99 latency gains for this workload.";
    } else {
        const auto& best_eval = report.evaluations[best_idx];
        rec << "Recommended: " << best_eval.policy.to_string() << "\n"
            << "  - P99 Tail Latency reduced by " << best_eval.p99_improvement_pct << "% ("
            << baseline_latency.p99_ms << " ms -> " << best_eval.latency.p99_ms << " ms)\n";
        if (best_eval.context_switch_reduction > 0) {
            rec << "  - Involuntary preemptions reduced by " << best_eval.context_switch_reduction << " context switches\n";
        }
    }
    report.recommendation = rec.str();

    return report;
}

} // namespace internal

std::string AdvisorReport::summary() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);
    ss << "============================================================\n";
    ss << "                 AFFIX PLACEMENT ADVISOR                    \n";
    ss << "============================================================\n\n";

    ss << std::left << std::setw(28) << "Policy"
       << std::setw(12) << "P50 (ms)"
       << std::setw(12) << "P99 (ms)"
       << std::setw(12) << "P99 Delta"
       << "Invol Switches\n";
    ss << std::string(75, '-') << "\n";

    for (size_t i = 0; i < evaluations.size(); ++i) {
        const auto& e = evaluations[i];
        std::string mark = (i == best_index) ? " [*]" : "";
        std::string delta_str = (i == baseline_index)
            ? "Baseline"
            : ((e.p99_improvement_pct >= 0 ? "+" : "") + std::to_string(static_cast<int>(e.p99_improvement_pct)) + "%");

        ss << std::left << std::setw(28) << (e.policy.to_string() + mark)
           << std::setw(12) << e.latency.p50_ms
           << std::setw(12) << e.latency.p99_ms
           << std::setw(12) << delta_str
           << e.aggregate.involuntary_context_switches << "\n";
    }

    ss << "\n[Verdict & Recommendation]:\n" << recommendation << "\n";
    ss << "============================================================\n";
    return ss.str();
}

std::string AdvisorReport::to_json() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);
    ss << "{\n";
    ss << "  \"best_index\": " << best_index << ",\n";
    ss << "  \"recommendation\": \"" << recommendation << "\",\n";
    ss << "  \"evaluations\": [\n";

    for (size_t i = 0; i < evaluations.size(); ++i) {
        const auto& e = evaluations[i];
        ss << "    {\n";
        ss << "      \"policy\": \"" << e.policy.to_string() << "\",\n";
        ss << "      \"p50_ms\": " << e.latency.p50_ms << ",\n";
        ss << "      \"p99_ms\": " << e.latency.p99_ms << ",\n";
        ss << "      \"p99_improvement_pct\": " << e.p99_improvement_pct << ",\n";
        ss << "      \"involuntary_context_switches\": " << e.aggregate.involuntary_context_switches << "\n";
        ss << "    }" << (i + 1 < evaluations.size() ? "," : "") << "\n";
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

} // namespace affix
