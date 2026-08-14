#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <sstream>
#include <chrono>

namespace nextviper::test {

struct TestCase {
    std::string suite;
    std::string name;
    std::function<void()> func;
};

class TestRegistry {
public:
    static TestRegistry& instance() {
        static TestRegistry reg;
        return reg;
    }

    void register_test(std::string suite, std::string name, std::function<void()> func) {
        tests_.push_back({std::move(suite), std::move(name), std::move(func)});
    }

    int run_all() {
        size_t passed = 0;
        size_t failed = 0;
        std::string current_suite = "";

        std::cout << "\033[1;36m====================================================\033[0m\n";
        std::cout << "\033[1;35m  NextViper 0.1.0 Test Suite Runner\033[0m\n";
        std::cout << "\033[1;36m====================================================\033[0m\n\n";

        auto start_time = std::chrono::high_resolution_clock::now();

        for (const auto& test : tests_) {
            if (test.suite != current_suite) {
                current_suite = test.suite;
                std::cout << "\033[1m[" << current_suite << "]\033[0m\n" << std::flush;
            }

            try {
                test.func();
                std::cout << "  \033[1;32m✓ PASS\033[0m: " << test.name << "\n" << std::flush;
                passed++;
            } catch (const std::exception& e) {
                std::cout << "  \033[1;31m✗ FAIL\033[0m: " << test.name << "\n" << std::flush;
                std::cout << "    \033[31mError: " << e.what() << "\033[0m\n" << std::flush;
                failed++;
            } catch (...) {
                std::cout << "  \033[1;31m✗ FAIL\033[0m: " << test.name << " (unknown exception)\n" << std::flush;
                failed++;
            }
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        double elapsed_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();

        std::cout << "\n\033[1;36m----------------------------------------------------\033[0m\n";
        std::cout << "Tests Summary: " << (passed + failed) << " total | "
                  << "\033[1;32m" << passed << " passed\033[0m | "
                  << (failed > 0 ? "\033[1;31m" : "") << failed << " failed"
                  << (failed > 0 ? "\033[0m" : "")
                  << " (" << elapsed_ms << " ms)\n";
        std::cout << "\033[1;36m====================================================\033[0m\n";

        return (failed == 0) ? 0 : 1;
    }

private:
    std::vector<TestCase> tests_;
};

struct TestRegistrar {
    TestRegistrar(std::string suite, std::string name, std::function<void()> func) {
        TestRegistry::instance().register_test(std::move(suite), std::move(name), std::move(func));
    }
};

#define NV_TEST(suite, name) \
    static void test_##suite##_##name(); \
    static nextviper::test::TestRegistrar registrar_##suite##_##name(#suite, #name, test_##suite##_##name); \
    static void test_##suite##_##name()

#define NV_ASSERT(condition) \
    do { \
        if (!(condition)) { \
            std::ostringstream ss; \
            ss << "Assertion failed: (" #condition ") at " << __FILE__ << ":" << __LINE__; \
            throw std::runtime_error(ss.str()); \
        } \
    } while (0)

template<typename T>
concept StreamPrintable = requires(std::ostream& os, const T& val) {
    os << val;
};

template<typename T>
std::string stringify(const T& val) {
    if constexpr (StreamPrintable<T>) {
        std::ostringstream ss;
        ss << val;
        return ss.str();
    } else {
        return "<value>";
    }
}

#define NV_ASSERT_EQ(actual, expected) \
    do { \
        if (!((actual) == (expected))) { \
            std::ostringstream ss; \
            ss << "Equality assertion failed at " << __FILE__ << ":" << __LINE__ \
               << "\n      Expected: " << nextviper::test::stringify(expected) \
               << "\n      Actual:   " << nextviper::test::stringify(actual); \
            throw std::runtime_error(ss.str()); \
        } \
    } while (0)

#define NV_ASSERT_TRUE(val) NV_ASSERT((val) == true)
#define NV_ASSERT_FALSE(val) NV_ASSERT((val) == false)

} // namespace nextviper::test
