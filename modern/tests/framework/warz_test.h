// A minimal self-registering test framework.
//
// WHY NOT doctest / GoogleTest: this tree builds in containers with no package
// registry reachable at configure time, and both of those would arrive through a CPM
// fetch that fails there. It also keeps DEPENDENCIES.md unchanged -- the licence audit
// is a deliverable of this port, and a test framework is not worth an entry in it.
//
// It has to work identically under two toolchains: i686-w64-mingw32-g++ (the shipping
// target) and the host g++ (the only one that can currently RUN anything -- see
// tests/CMakeLists.txt). So: no Win32, no POSIX, no RTTI beyond what GCC gives for
// free, C++20 and the C++ standard library only.

#ifndef WARZ_TEST_H
#define WARZ_TEST_H

#include <cmath>
#include <concepts>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace warz_test {

// Thrown by REQUIRE-family assertions to abandon the current test case. CHECK-family
// assertions record a failure and carry on, which is what you want when a test makes
// twenty independent assertions and you would rather see all twenty results.
struct AbortTestCase {};

struct TestCase {
    const char* suite;
    const char* name;
    const char* file;
    int         line;
    void      (*fn)();
};

// Registry. A function-local static, not a namespace-scope one, because test cases
// register from static initialisers across many translation units and the static
// initialisation order between them is unspecified.
std::vector<TestCase>& registry();

struct Registrar {
    Registrar(const char* suite, const char* name, const char* file, int line, void (*fn)())
    {
        registry().push_back(TestCase{suite, name, file, line, fn});
    }
};

// Per-run counters, owned by the runner.
struct Stats {
    int cases_run    = 0;
    int cases_failed = 0;
    int checks_run   = 0;
    int checks_failed = 0;
};

Stats& stats();

// Records a failure against the current test case and prints it. Returns false so the
// assertion macros can use it in an expression.
bool report_failure(const char* file, int line, const char* expr, const std::string& detail);

// ---------------------------------------------------------------------------
// Comparison helpers
//
// Free functions rather than macro-internal expressions so that operands are evaluated
// exactly once -- CHECK_EQ(next(), 3) must not call next() twice.
// ---------------------------------------------------------------------------

std::string to_string(long long v);
std::string to_string(unsigned long long v);
std::string to_string(double v);
std::string to_string(const char* v);
std::string to_string(const std::string& v);
inline std::string to_string(int v)                { return to_string(static_cast<long long>(v)); }
inline std::string to_string(long v)               { return to_string(static_cast<long long>(v)); }
inline std::string to_string(unsigned v)           { return to_string(static_cast<unsigned long long>(v)); }
inline std::string to_string(unsigned long v)      { return to_string(static_cast<unsigned long long>(v)); }
inline std::string to_string(float v)              { return to_string(static_cast<double>(v)); }
inline std::string to_string(bool v)               { return v ? "true" : "false"; }
template <typename T> std::string to_string(T* v)  { return v ? "<non-null pointer>" : "nullptr"; }

// Fallback for everything else.
//
// A test framework must never fail to COMPILE because someone compared two values of a
// domain type -- the whole point is to make writing a test cheap. The engine is full of
// small wrapper types that a naive overload set does not cover: gobjid_t (GameObj.h:169)
// wraps a DWORD, and r3dSec_type wraps whatever it is protecting.
//
// Wrappers exposing an integral get() are printed by their value, which is what makes a
// failure message about object ids readable. Anything else degrades to a placeholder --
// the comparison still runs and still reports, it just cannot say what the values were.
template <typename T>
concept HasIntegralGet = requires(const T& t) {
    { t.get() } -> std::convertible_to<long long>;
};

template <typename T>
std::string to_string(const T& v)
{
    if constexpr (HasIntegralGet<T>)
        return to_string(static_cast<long long>(v.get()));
    else
        return "<value of a type the harness cannot print>";
}

// Absolute-or-relative tolerance. A pure absolute epsilon is wrong for matrix entries
// that legitimately reach into the thousands (a far plane, a world-space translation);
// a pure relative one is wrong near zero, which is most of a rotation matrix.
inline bool nearly_equal(double a, double b, double eps)
{
    if (std::isnan(a) || std::isnan(b)) return false;
    if (a == b) return true;
    const double diff  = std::fabs(a - b);
    if (diff <= eps) return true;
    const double scale = std::fmax(std::fabs(a), std::fabs(b));
    return diff <= eps * scale;
}

// The runner. Returns a process exit code: 0 all passed, 1 something failed.
int run(int argc, char** argv);

} // namespace warz_test

// ---------------------------------------------------------------------------
// Macros
// ---------------------------------------------------------------------------

#define WARZ_TEST_CAT2(a, b) a##b
#define WARZ_TEST_CAT(a, b)  WARZ_TEST_CAT2(a, b)

// WARZ_TEST(suite, name) { ...body... }
//
// Defines and registers a test case. `suite` and `name` are bare tokens, stringified
// here, so they read as identifiers at the call site and as text in the report.
#define WARZ_TEST(suite, name)                                                        \
    static void WARZ_TEST_CAT(warz_test_fn_, __LINE__)();                             \
    static ::warz_test::Registrar WARZ_TEST_CAT(warz_test_reg_, __LINE__)(            \
        #suite, #name, __FILE__, __LINE__, &WARZ_TEST_CAT(warz_test_fn_, __LINE__));  \
    static void WARZ_TEST_CAT(warz_test_fn_, __LINE__)()

#define WARZ_CHECK_IMPL(expr, detail, on_fail)                                        \
    do {                                                                              \
        ++::warz_test::stats().checks_run;                                            \
        if (!(expr)) {                                                                \
            ::warz_test::report_failure(__FILE__, __LINE__, #expr, (detail));         \
            on_fail;                                                                  \
        }                                                                             \
    } while (0)

// CHECK: record and continue.  REQUIRE: record and abandon this test case.
#define CHECK(expr)   WARZ_CHECK_IMPL(expr, std::string(), (void)0)
#define REQUIRE(expr) WARZ_CHECK_IMPL(expr, std::string(), throw ::warz_test::AbortTestCase{})

#define WARZ_BINARY_CHECK(a, b, op, on_fail)                                          \
    do {                                                                              \
        ++::warz_test::stats().checks_run;                                            \
        auto&& warz_a_ = (a);                                                         \
        auto&& warz_b_ = (b);                                                         \
        if (!(warz_a_ op warz_b_)) {                                                  \
            ::warz_test::report_failure(                                              \
                __FILE__, __LINE__, #a " " #op " " #b,                                \
                "got " + ::warz_test::to_string(warz_a_) +                            \
                ", expected " #op " " + ::warz_test::to_string(warz_b_));             \
            on_fail;                                                                  \
        }                                                                             \
    } while (0)

#define CHECK_EQ(a, b)   WARZ_BINARY_CHECK(a, b, ==, (void)0)
#define CHECK_NE(a, b)   WARZ_BINARY_CHECK(a, b, !=, (void)0)
#define CHECK_LT(a, b)   WARZ_BINARY_CHECK(a, b, <,  (void)0)
#define CHECK_LE(a, b)   WARZ_BINARY_CHECK(a, b, <=, (void)0)
#define CHECK_GT(a, b)   WARZ_BINARY_CHECK(a, b, >,  (void)0)
#define CHECK_GE(a, b)   WARZ_BINARY_CHECK(a, b, >=, (void)0)
#define REQUIRE_EQ(a, b) WARZ_BINARY_CHECK(a, b, ==, throw ::warz_test::AbortTestCase{})

// Floating point. Default tolerance is 1e-5 relative-or-absolute: these are float32
// pipelines, so ~7 significant decimal digits is all there ever was.
#define CHECK_NEAR_EPS(a, b, eps)                                                     \
    do {                                                                              \
        ++::warz_test::stats().checks_run;                                            \
        const double warz_a_ = static_cast<double>(a);                                \
        const double warz_b_ = static_cast<double>(b);                                \
        if (!::warz_test::nearly_equal(warz_a_, warz_b_, (eps))) {                    \
            ::warz_test::report_failure(                                              \
                __FILE__, __LINE__, #a " ~= " #b,                                     \
                "got " + ::warz_test::to_string(warz_a_) +                            \
                ", expected " + ::warz_test::to_string(warz_b_) +                     \
                " (tolerance " + ::warz_test::to_string(static_cast<double>(eps)) + ")"); \
        }                                                                             \
    } while (0)

#define CHECK_NEAR(a, b) CHECK_NEAR_EPS(a, b, 1e-5)

// Explicit failure, for the "should not be reached" arm of a test.
#define WARZ_FAIL(msg)                                                                \
    do {                                                                              \
        ++::warz_test::stats().checks_run;                                            \
        ::warz_test::report_failure(__FILE__, __LINE__, "explicit failure", (msg));   \
    } while (0)

#endif // WARZ_TEST_H
