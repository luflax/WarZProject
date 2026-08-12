// Self-test for the harness. If these fail, no other result in the suite means
// anything -- a framework whose CHECK silently passes reports a green suite forever.

#include "warz_test.h"

WARZ_TEST(framework, comparisons_hold)
{
    CHECK(true);
    CHECK_EQ(2 + 2, 4);
    CHECK_NE(1, 2);
    CHECK_LT(1, 2);
    CHECK_LE(2, 2);
    CHECK_GT(2, 1);
    CHECK_GE(2, 2);
}

WARZ_TEST(framework, operands_are_evaluated_once)
{
    // The binary-comparison macro binds each operand to a reference before comparing,
    // precisely so this holds. Written the obvious way -- expanding `a` and `b` twice
    // in the failure message -- this counter reaches 2 and every test with a side
    // effect in an assertion becomes a liar.
    int calls = 0;
    auto next = [&calls]() { return ++calls; };

    CHECK_EQ(next(), 1);
    CHECK_EQ(calls, 1);
}

WARZ_TEST(framework, float_tolerance_is_relative_and_absolute)
{
    // Absolute, near zero: a pure relative epsilon divides by ~0 and rejects these.
    CHECK_NEAR(0.0, 1e-9);
    CHECK_NEAR(1e-9, 0.0);

    // Relative, far from zero: 1e4 carries ~1e-3 of float32 error, which a pure
    // absolute 1e-5 epsilon would reject. Matrix translations reach this range.
    CHECK_NEAR(10000.0, 10000.00001);

    // And it still discriminates.
    CHECK(!::warz_test::nearly_equal(1.0, 1.1, 1e-5));
    CHECK(!::warz_test::nearly_equal(0.0, 1.0, 1e-5));
}

WARZ_TEST(framework, nan_never_compares_equal)
{
    // std::isnan short-circuit. Without it, `a == b` is false and the relative branch
    // computes NaN <= NaN, also false -- so this happens to work, but only by accident,
    // and the explicit check is what makes it intentional.
    const double nan = std::nan("");
    CHECK(!::warz_test::nearly_equal(nan, nan, 1e-5));
    CHECK(!::warz_test::nearly_equal(nan, 1.0, 1e-5));
    CHECK(!::warz_test::nearly_equal(1.0, nan, 1e-5));
}
