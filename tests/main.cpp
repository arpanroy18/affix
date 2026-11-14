#include "test_runner.hpp"
#include "corepilot/corepilot.hpp"
#include <thread>
#include <chrono>

TEST_CASE(cpuset_basic_operations) {
    corepilot::CpuSet set;
    ASSERT_TRUE(set.empty());
    ASSERT_EQ(set.count(), 0);

    set.set(2);
    set.set(4);
    ASSERT_FALSE(set.empty());
    ASSERT_EQ(set.count(), 2);
    ASSERT_TRUE(set.test(2));
    ASSERT_TRUE(set.test(4));
    ASSERT_FALSE(set.test(3));

    set.reset(2);
    ASSERT_EQ(set.count(), 1);
    ASSERT_FALSE(set.test(2));
    ASSERT_TRUE(set.test(4));

    set.clear();
    ASSERT_TRUE(set.empty());
}

TEST_CASE(cpuset_string_serialization) {
    corepilot::CpuSet set{0, 1, 2, 4, 6, 7, 8};
    std::string str = set.to_string();
    ASSERT_EQ(str, "0-2,4,6-8");

    corepilot::CpuSet parsed = corepilot::CpuSet::from_string("0-2,4,6-8");
    ASSERT_EQ(set, parsed);

    corepilot::CpuSet single = corepilot::CpuSet::from_string("5");
    ASSERT_EQ(single.count(), 1);
    ASSERT_TRUE(single.test(5));
}

TEST_CASE(cpuset_bitwise_ops) {
    corepilot::CpuSet a{0, 1, 2};
    corepilot::CpuSet b{1, 2, 3};

    corepilot::CpuSet intersection = a & b;
    ASSERT_EQ(intersection.count(), 2);
    ASSERT_TRUE(intersection.test(1));
    ASSERT_TRUE(intersection.test(2));

    corepilot::CpuSet union_set = a | b;
    ASSERT_EQ(union_set.count(), 4);
    ASSERT_TRUE(union_set.test(0));
    ASSERT_TRUE(union_set.test(3));
}

TEST_CASE(topology_discovery) {
    const auto& topo = corepilot::get_system_topology();
    ASSERT_TRUE(topo.logical_core_count() > 0);
    ASSERT_TRUE(topo.physical_core_count() > 0);
    ASSERT_FALSE(topo.allowed_cpus().empty());

    const auto* core0 = topo.get_core(0);
    ASSERT_NE(core0, nullptr);
    ASSERT_EQ(core0->logical_id, 0);

    std::string summary = topo.summary();
    ASSERT_FALSE(summary.empty());

    std::string json = topo.to_json();
    ASSERT_FALSE(json.empty());
}

TEST_CASE(policy_resolution) {
    const auto& topo = corepilot::get_system_topology();

    auto pinned = corepilot::Policy::pinned(0);
    auto mask = pinned.resolve(topo);
    ASSERT_TRUE(mask.test(0));
    ASSERT_EQ(mask.count(), 1);

    auto unres = corepilot::Policy::unrestricted();
    auto unres_mask = unres.resolve(topo);
    ASSERT_EQ(unres_mask, topo.allowed_cpus());
}

TEST_CASE(scoped_placement) {
    auto initial_mask = corepilot::this_thread::get_placement();
    ASSERT_FALSE(initial_mask.empty());
    {
        corepilot::ScopedPlacement guard(corepilot::Policy::pinned(0));
        ASSERT_TRUE(guard.applied());
    }
    // Verified that guard restores or keeps valid mask
    auto after_mask = corepilot::this_thread::get_placement();
    ASSERT_FALSE(after_mask.empty());
}

TEST_CASE(thread_handle_placement) {
    std::thread t([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    });

    auto res = corepilot::apply_placement(t, corepilot::Policy::pinned(0));
    ASSERT_TRUE(res.applied);
    t.join();
}

TEST_CASE(metrics_capture_and_probe) {
    corepilot::MetricsSnapshot snapshot;
    {
        corepilot::ScopedProbe probe(&snapshot);
        volatile double sum = 0.0;
        for (int i = 0; i < 100'000; ++i) {
            sum += i * 0.001;
        }
    }

    ASSERT_TRUE(snapshot.wall_time_ns > 0);
    std::string json = snapshot.to_json();
    ASSERT_FALSE(json.empty());
}

TEST_CASE(telemetry_session_aggregation) {
    corepilot::TelemetrySession session("test_session");

    for (int i = 0; i < 10; ++i) {
        corepilot::MetricsSnapshot s;
        s.wall_time_ns = (i + 1) * 1'000'000; // 1ms, 2ms, ...
        s.cpu_time_ns = (i + 1) * 900'000;
        session.record(s);
    }

    ASSERT_EQ(session.sample_count(), 10);
    auto stats = session.wall_time_stats();
    ASSERT_EQ(stats.sample_count, 10);
    ASSERT_EQ(stats.min_ms, 1.0);
    ASSERT_EQ(stats.max_ms, 10.0);

    auto agg = session.aggregate_metrics();
    ASSERT_EQ(agg.wall_time_ns, 55'000'000);
}

TEST_CASE(workload_execution) {
    corepilot::Workload wl("compute_workload", corepilot::Policy::pinned(0));

    int result = wl.run([]() {
        int sum = 0;
        for (int i = 0; i < 50'000; ++i) sum += (i % 7);
        return sum;
    });

    ASSERT_NE(result, 0);
    ASSERT_TRUE(wl.last_metrics().wall_time_ns > 0);
    ASSERT_EQ(wl.session().sample_count(), 1);
}

int main() {
    return test::run_all_tests();
}
