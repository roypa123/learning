// libaudio - test.h
// A minimal test harness. See Chapter 17, section 17.7.
//
//     CHECK(peak(buf) < 1.0);
//     CHECK_CLOSE(rms(buf), 0.70710678, 0.001);
//     return audio::test::summary();

#pragma once

#include <iostream>
#include <iomanip>
#include <cmath>
#include <string>

namespace audio { namespace test {

// C++17 inline variables: shared across translation units, no .cpp needed.
inline int g_passed = 0;
inline int g_failed = 0;

inline void check(bool condition, const std::string& what,
                  const char* file, int line)
{
    if (condition)
    {
        ++g_passed;
    }
    else
    {
        ++g_failed;
        std::cout << "  FAIL  " << what << "\n"
                  << "        at " << file << ":" << line << "\n";
    }
}

inline void checkClose(double a, double b, double tolerance,
                       const std::string& what, const char* file, int line)
{
    const bool ok = std::fabs(a - b) <= tolerance;
    if (ok)
    {
        ++g_passed;
    }
    else
    {
        ++g_failed;
        std::cout << "  FAIL  " << what << "\n"
                  << "        expected " << b << ", got " << a
                  << " (difference " << std::fabs(a - b)
                  << ", tolerance " << tolerance << ")\n"
                  << "        at " << file << ":" << line << "\n";
    }
}

inline void section(const std::string& title)
{
    std::cout << title << "\n";
}

inline int summary()
{
    std::cout << "\n" << g_passed << " passed, " << g_failed << " failed.\n";
    return g_failed == 0 ? 0 : 1;
}

}}   // namespace audio::test

// Macros, not functions: __FILE__ and __LINE__ must expand at the CALL SITE,
// and #cond stringifies the literal text of the condition.
#define CHECK(cond)            audio::test::check((cond), #cond, __FILE__, __LINE__)
#define CHECK_CLOSE(a, b, tol) audio::test::checkClose((a), (b), (tol), \
                                   #a " ~= " #b, __FILE__, __LINE__)
