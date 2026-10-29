#pragma once
#include <iostream>

inline int& failureCount() { static int f = 0; return f; }

#define CHECK(cond) do { if (!(cond)) { \
    std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " << #cond << "\n"; ++failureCount(); } } while (0)

#define CHECK_THROWS(expr) do { bool threw_ = false; \
    try { expr; } catch (const std::exception&) { threw_ = true; } \
    CHECK(threw_); } while (0)

inline int finishTests(const char* suite) {
    if (failureCount()) { std::cerr << suite << ": " << failureCount() << " check(s) FAILED\n"; return 1; }
    std::cout << suite << ": all checks passed\n";
    return 0;
}
