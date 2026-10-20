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

