#include "test_runner.hpp"
#include "affix/affix.hpp"
#include <thread>
#include <chrono>

TEST_CASE(cpuset_basic_operations) {
    affix::CpuSet set;
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
    affix::CpuSet set{0, 1, 2, 4, 6, 7, 8};
    std::string str = set.to_string();
    ASSERT_EQ(str, "0-2,4,6-8");

    affix::CpuSet parsed = affix::CpuSet::from_string("0-2,4,6-8");
    ASSERT_EQ(set, parsed);

    affix::CpuSet single = affix::CpuSet::from_string("5");
    ASSERT_EQ(single.count(), 1);
    ASSERT_TRUE(single.test(5));
}

TEST_CASE(cpuset_bitwise_ops) {
    affix::CpuSet a{0, 1, 2};
    affix::CpuSet b{1, 2, 3};

    affix::CpuSet intersection = a & b;
    ASSERT_EQ(intersection.count(), 2);
    ASSERT_TRUE(intersection.test(1));
    ASSERT_TRUE(intersection.test(2));

    affix::CpuSet union_set = a | b;
    ASSERT_EQ(union_set.count(), 4);
    ASSERT_TRUE(union_set.test(0));
    ASSERT_TRUE(union_set.test(3));
}

TEST_CASE(topology_discovery) {
    const auto& topo = affix::get_system_topology();
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
    const auto& topo = affix::get_system_topology();

    auto pinned = affix::Policy::pinned(0);
    auto mask = pinned.resolve(topo);
    ASSERT_TRUE(mask.test(0));
    ASSERT_EQ(mask.count(), 1);

    auto unres = affix::Policy::unrestricted();
    auto unres_mask = unres.resolve(topo);
    ASSERT_EQ(unres_mask, topo.allowed_cpus());
}

TEST_CASE(scoped_placement) {
    auto initial_mask = affix::this_thread::get_placement();
    ASSERT_FALSE(initial_mask.empty());
    {
        affix::ScopedPlacement guard(affix::Policy::pinned(0));
        ASSERT_TRUE(guard.applied());
    }
    // Verified that guard restores or keeps valid mask
    auto after_mask = affix::this_thread::get_placement();
    ASSERT_FALSE(after_mask.empty());
}

TEST_CASE(thread_handle_placement) {
    std::thread t([]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    });

    auto res = affix::apply_placement(t, affix::Policy::pinned(0));
    ASSERT_TRUE(res.applied);
    t.join();
}

TEST_CASE(metrics_capture_and_probe) {
    affix::MetricsSnapshot snapshot;
    {
        affix::ScopedProbe probe(&snapshot);
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
    affix::TelemetrySession session("test_session");

    for (int i = 0; i < 10; ++i) {
        affix::MetricsSnapshot s;
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
    affix::Workload wl("compute_workload", affix::Policy::pinned(0));

    int result = wl.run([]() {
        int sum = 0;
        for (int i = 0; i < 50'000; ++i) sum += (i % 7);
        return sum;
    });

    ASSERT_NE(result, 0);
    ASSERT_TRUE(wl.last_metrics().wall_time_ns > 0);
    ASSERT_EQ(wl.session().sample_count(), 1);
}

TEST_CASE(realtime_priority_policy) {
    auto policy = affix::Policy::pinned(0).with_realtime_fifo(70);
    ASSERT_EQ(policy.sched_priority(), affix::SchedPriority::RealtimeFifo);
    ASSERT_EQ(policy.priority_value(), 70);

    affix::ScopedPlacement guard(policy);
    ASSERT_TRUE(guard.applied());
}

TEST_CASE(isolated_core_policy) {
    const auto& topo = affix::get_system_topology();
    auto policy = affix::Policy::isolated_core();
    auto resolved = policy.resolve(topo);
    ASSERT_FALSE(resolved.empty());
}

TEST_CASE(numa_allocation_and_stl_allocator) {
    void* raw = affix::numa::allocate(4096, 0);
    ASSERT_NE(raw, nullptr);
    affix::numa::free(raw, 4096);

    std::vector<int, affix::numa::NumaAllocator<int>> vec(affix::numa::NumaAllocator<int>(0));
    for (int i = 0; i < 1000; ++i) {
        vec.push_back(i * 2);
    }
    ASSERT_EQ(vec.size(), 1000);
    ASSERT_EQ(vec[500], 1000);
}

TEST_CASE(core_warmup_and_spin_pause) {
    affix::warmup_core(std::chrono::milliseconds(2));
    affix::spin_pause(100);
    affix::this_thread::warmup(std::chrono::milliseconds(1));
    ASSERT_TRUE(true);
}

TEST_CASE(placement_advisor_evaluation) {
    std::vector<affix::Policy> policies = {
        affix::Policy::unrestricted(),
        affix::Policy::pinned(0),
        affix::Policy::performance_cores()
    };

    auto work = []() {
        volatile double x = 1.0;
        for (int i = 0; i < 10'000; ++i) {
            x = x * 1.0001 + 0.001;
        }
    };

    auto report = affix::evaluate_placement(work, policies, 10, 2);
    ASSERT_EQ(report.evaluations.size(), 3);
    ASSERT_FALSE(report.recommendation.empty());
    ASSERT_FALSE(report.summary().empty());
    ASSERT_FALSE(report.to_json().empty());
}

TEST_CASE(cpuset_from_string_adversarial) {
    auto parsed = affix::CpuSet::from_string("0-2, 4 , 6-8,10,,bogus,3-3");
    ASSERT_TRUE(parsed.test(0));
    ASSERT_TRUE(parsed.test(3));
    ASSERT_TRUE(parsed.test(4));
    ASSERT_TRUE(parsed.test(6));
    ASSERT_TRUE(parsed.test(10));
    ASSERT_EQ(parsed.count(), 9);
}

TEST_CASE(metrics_snapshot_delta_arithmetic) {
    affix::MetricsSnapshot before;
    before.wall_time_ns = 100;
    before.cpu_time_ns = 40;
    affix::MetricsSnapshot after = before;
    after.wall_time_ns = 350;
    after.cpu_time_ns = 190;
    auto delta = after - before;
    ASSERT_EQ(delta.wall_time_ns, 250);
    ASSERT_EQ(delta.cpu_time_ns, 150);
}

int main() {
    return test::run_all_tests();
}
