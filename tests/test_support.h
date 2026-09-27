#ifndef DOUDIZHU_TEST_SUPPORT_H
#define DOUDIZHU_TEST_SUPPORT_H

#include <iostream>
#include <iterator>

#define EXPECT_TRUE(condition)                                                   \
    do {                                                                         \
        if (!(condition)) {                                                       \
            std::cerr << __FILE__ << ':' << __LINE__                             \
                      << " expected true: " #condition << '\n';                  \
            return false;                                                        \
        }                                                                        \
    } while (false)

#define EXPECT_EQ(actual, expected)                                              \
    do {                                                                         \
        const auto actualValue = (actual);                                        \
        const auto expectedValue = (expected);                                    \
        if (!(actualValue == expectedValue)) {                                    \
            std::cerr << __FILE__ << ':' << __LINE__                             \
                      << " values differ: " #actual " != " #expected << '\n';   \
            return false;                                                        \
        }                                                                        \
    } while (false)

struct TestCase
{
    const char *name;
    bool (*run)();
};

inline int runTests(const TestCase *tests, int count)
{
    for (int i = 0; i < count; ++i) {
        if (!tests[i].run()) {
            std::cerr << "FAIL: " << tests[i].name << '\n';
            return 1;
        }
        std::cout << "PASS: " << tests[i].name << '\n';
    }
    return 0;
}

#endif // DOUDIZHU_TEST_SUPPORT_H
