#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <stdexcept>

namespace test {

struct TestCase {
    std::string name;
    std::function<void()> func;
};

inline std::vector<TestCase>& get_registry() {
    static std::vector<TestCase> registry;
    return registry;
}

inline void register_test(const std::string& name, std::function<void()> func) {
    get_registry().push_back({name, std::move(func)});
}

#define TEST_CASE(name) \
    static void test_##name(); \
    namespace { \
        struct Reg_##name { \
            Reg_##name() { ::test::register_test(#name, test_##name); } \
        } global_reg_##name; \
    } \
    static void test_##name()

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            throw std::runtime_error(std::string("Assertion failed: ") + #cond + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
        } \
    } while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_EQ(a, b) \
    do { \
        if (!((a) == (b))) { \
            throw std::runtime_error(std::string("Assertion failed: ") + #a + " == " + #b + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
        } \
    } while (0)

#define ASSERT_NE(a, b) \
    do { \
        if ((a) == (b)) { \
            throw std::runtime_error(std::string("Assertion failed: ") + #a + " != " + #b + " at " + __FILE__ + ":" + std::to_string(__LINE__)); \
        } \
    } while (0)

inline int run_all_tests() {
    auto& tests = get_registry();
    std::cout << "Running " << tests.size() << " tests...\n";
    size_t passed = 0;
    size_t failed = 0;

    for (const auto& t : tests) {
        try {
            t.func();
            std::cout << "  [PASS] " << t.name << "\n";
            passed++;
        } catch (const std::exception& e) {
            std::cout << "  [FAIL] " << t.name << " : " << e.what() << "\n";
            failed++;
        } catch (...) {
            std::cout << "  [FAIL] " << t.name << " : Unknown exception\n";
            failed++;
        }
    }

    std::cout << "\nResults: " << passed << " passed, " << failed << " failed.\n";
    return (failed == 0) ? 0 : 1;
}

} // namespace test
