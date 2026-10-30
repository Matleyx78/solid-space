// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Unit tests of the exact numbers of geom2d (stage 0.2, package P6). The
// expected values come from IEEE 754 itself (the hardware rounds each single
// operation correctly), from identities that hold for exact arithmetic, and
// from values worked out by hand; never from running the code under test.

#include "exact/dyadic.hpp"
#include "exact/quadratic.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <vector>

namespace {

using solid_space::geom2d::exact::Dyadic;
using solid_space::geom2d::exact::QuadraticNumber;
using solid_space::robust::Sign;

namespace exact = solid_space::geom2d::exact;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kMinSubnormal = std::numeric_limits<double>::denorm_min();
constexpr double kMaxDouble = std::numeric_limits<double>::max();

// SplitMix64: a deterministic stream of 64-bit values, the same on every
// platform, so that every run checks the same data.
class Generator {
public:
    explicit Generator(std::uint64_t seed) : m_state(seed) {}

    std::uint64_t next() {
        m_state += 0x9E3779B97F4A7C15ULL;
        std::uint64_t z = m_state;
        z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31U);
    }

    // A finite double with a uniformly chosen biased exponent field, so that
    // subnormals and every binade appear as often as each other.
    double finite() {
        const std::uint64_t bits = next();
        const std::uint64_t field = (bits >> 52U) % 2047U;
        return std::bit_cast<double>((bits & 0x800FFFFFFFFFFFFFULL) | (field << 52U));
    }

    // A finite double with magnitude in [2^low, 2^high), and a random sign.
    double inBinades(int low, int high) {
        const std::uint64_t bits = next();
        const auto span = static_cast<std::uint64_t>(high - low);
        const auto exponent = low + static_cast<int>((bits >> 53U) % span);
        const double significand = 1.0 + static_cast<double>(bits & 0xFFFFFFFFFFFFFULL) * 0x1p-52;
        const double value = std::ldexp(significand, exponent);
        return (bits & 1U) != 0 ? -value : value;
    }

private:
    std::uint64_t m_state;
};

std::uint64_t bitsOf(double value) {
    return std::bit_cast<std::uint64_t>(value);
}

// Bitwise equality, so that -0.0 and +0.0 differ.
::testing::AssertionResult sameDouble(double actual, double expected) {
    if (bitsOf(actual) == bitsOf(expected)) {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure() << std::hexfloat << actual << " instead of " << expected;
}

Dyadic d(double value) {
    return Dyadic{value};
}

Dyadic powerOfTwo(std::int64_t exponent) {
    return Dyadic{1.0}.timesPowerOfTwo(exponent);
}

// --- Dyadic -----------------------------------------------------------------

TEST(Dyadic, EveryFiniteDoubleComesBack) {
    const std::vector<double> special = {0.0,
                                         -0.0,
                                         kMinSubnormal,
                                         -kMinSubnormal,
                                         0x1.fffffffffffffp-1023,
                                         std::numeric_limits<double>::min(),
                                         kMaxDouble,
                                         -kMaxDouble,
                                         1.0,
                                         -1.0,
                                         0x1p-1074 * 3.0,
                                         0x1p1023,
                                         0x1p-1022 - kMinSubnormal};
    for (const double value : special) {
        EXPECT_TRUE(sameDouble(d(value).toDouble(), value)) << std::hexfloat << value;
    }
    // Every exponent field from 0 (zero and subnormals) to 2046, both signs,
    // several significands each.
    Generator generator{1};
    for (std::uint64_t field = 0; field <= 2046; ++field) {
        for (int i = 0; i < 8; ++i) {
            const std::uint64_t bits = (generator.next() & 0x800FFFFFFFFFFFFFULL) | (field << 52U);
            const auto value = std::bit_cast<double>(bits);
            ASSERT_TRUE(sameDouble(d(value).toDouble(), value)) << std::hexfloat << value;
        }
    }
}

TEST(Dyadic, SignsAndComparisons) {
    EXPECT_EQ(Dyadic{}.sign(), Sign::zero);
    EXPECT_EQ(d(-0.0).sign(), Sign::zero);
    EXPECT_EQ(d(kMinSubnormal).sign(), Sign::positive);
    EXPECT_EQ(d(-kMinSubnormal).sign(), Sign::negative);
    EXPECT_EQ(compare(d(0.0), d(-0.0)), Sign::zero);
    EXPECT_EQ(compare(d(1.0), d(0x1.0000000000001p0)), Sign::negative);
    EXPECT_EQ(compare(d(-1.0), d(-0x1.0000000000001p0)), Sign::positive);
    EXPECT_EQ(compare(d(kMinSubnormal), d(-kMaxDouble)), Sign::positive);
    EXPECT_EQ(compare(d(0x1p-1074), d(0x1p-1074)), Sign::zero);
    EXPECT_EQ(compare(Dyadic::fromInteger(3), d(3.0)), Sign::zero);
    EXPECT_EQ(compare(Dyadic::fromInteger(std::numeric_limits<std::int64_t>::min()), d(-0x1p63)),
              Sign::zero);
    // Exponents far beyond those of the doubles.
    EXPECT_EQ(compare(powerOfTwo(5000), powerOfTwo(4999) * d(3.0)), Sign::negative);
    EXPECT_EQ(compare(powerOfTwo(-5000), Dyadic{}), Sign::positive);
}

TEST(Dyadic, SingleOperationsRoundAsIeee) {
    // The hardware rounds +, - and * correctly: the exact result rounded by
    // toDouble must give the same bits, zeros, subnormals and overflows included.
    Generator generator{2};
    for (int i = 0; i < 20000; ++i) {
        const double x = generator.finite();
        const double y = generator.finite();
        const double sum = x + y;
        const double difference = x - y;
        const double product = x * y;
        ASSERT_TRUE(sameDouble((d(x) + d(y)).toDouble(), sum)) << std::hexfloat << x << ' ' << y;
        ASSERT_TRUE(sameDouble((d(x) - d(y)).toDouble(), difference))
            << std::hexfloat << x << ' ' << y;
        ASSERT_TRUE(sameDouble((d(x) * d(y)).toDouble(), product))
            << std::hexfloat << x << ' ' << y;
    }
    // Close exponents, where sums cancel and round most often.
    for (int i = 0; i < 20000; ++i) {
        const double x = generator.inBinades(-3, 3);
        const double y = generator.inBinades(-3, 3);
        const double sum = x + y;
        const double product = x * y;
        ASSERT_TRUE(sameDouble((d(x) + d(y)).toDouble(), sum)) << std::hexfloat << x << ' ' << y;
        ASSERT_TRUE(sameDouble((d(x) * d(y)).toDouble(), product))
            << std::hexfloat << x << ' ' << y;
    }
    // Zeros keep the IEEE signs.
    EXPECT_TRUE(sameDouble((d(-0.0) + d(-0.0)).toDouble(), -0.0));
    EXPECT_TRUE(sameDouble((d(-0.0) + d(0.0)).toDouble(), 0.0));
    EXPECT_TRUE(sameDouble((d(1.5) - d(1.5)).toDouble(), 0.0));
    EXPECT_TRUE(sameDouble((d(-0.0) * d(2.0)).toDouble(), -0.0));
    EXPECT_TRUE(sameDouble((-d(0.0)).toDouble(), -0.0));
}

TEST(Dyadic, IdentitiesHoldExactly) {
    Generator generator{3};
    for (int i = 0; i < 2000; ++i) {
        const Dyadic x = d(generator.finite());
        const Dyadic y = d(generator.finite());
        const Dyadic z = d(generator.finite());
        ASSERT_EQ(compare((x + y) * (x - y), x * x - y * y), Sign::zero);
        ASSERT_EQ(compare((x + y) + z, x + (y + z)), Sign::zero);
        ASSERT_EQ(compare((x * y) * z, x * (y * z)), Sign::zero);
        ASSERT_EQ(compare(x * (y + z), x * y + x * z), Sign::zero);
        ASSERT_EQ(compare(x - x, Dyadic{}), Sign::zero);
        ASSERT_EQ((x - y).sign(), compare(x, y));
    }
}

TEST(Dyadic, ProductsBelowTheSubnormalsKeepTheirSign) {
    const Dyadic tiny = d(kMinSubnormal);
    const Dyadic square = tiny * tiny;                       // 2^-2148
    const Dyadic negativeCube = square * d(-kMinSubnormal);  // -2^-3222
    EXPECT_EQ(square.sign(), Sign::positive);
    EXPECT_EQ(negativeCube.sign(), Sign::negative);
    EXPECT_EQ(compare(square, Dyadic{}), Sign::positive);
    EXPECT_EQ(compare(negativeCube, square), Sign::negative);
    // In doubles both are zeros, with the sign of the product.
    EXPECT_TRUE(sameDouble(square.toDouble(), 0.0));
    EXPECT_TRUE(sameDouble(negativeCube.toDouble(), -0.0));
    // Nothing was lost: scaled back, they are the exact values.
    EXPECT_TRUE(sameDouble(square.timesPowerOfTwo(2148).toDouble(), 1.0));
    EXPECT_TRUE(sameDouble(negativeCube.timesPowerOfTwo(3222).toDouble(), -1.0));
    // A difference of two such products is exact too.
    const Dyadic other = d(3.0 * kMinSubnormal) * d(kMinSubnormal);
    EXPECT_EQ(compare(other - square, square.timesPowerOfTwo(1)), Sign::zero);
    // Products beyond the largest double.
    const Dyadic huge = d(kMaxDouble) * d(kMaxDouble);
    EXPECT_EQ(compare(huge, d(kMaxDouble)), Sign::positive);
    EXPECT_TRUE(sameDouble(huge.toDouble(), kInfinity));
    EXPECT_TRUE(sameDouble((-huge).toDouble(), -kInfinity));
}

TEST(Dyadic, ToDoubleRoundsToNearestWithTiesToEven) {
    const Dyadic one = d(1.0);
    // Halfway between 1 and its successor: to the even neighbour, 1.
    EXPECT_TRUE(sameDouble((one + powerOfTwo(-53)).toDouble(), 1.0));
    // Halfway between 1 + 2^-52 and 1 + 2^-51: to the even one, 1 + 2^-51.
    EXPECT_TRUE(sameDouble((one + d(3.0) * powerOfTwo(-53)).toDouble(), 0x1.0000000000002p0));
    // Just above halfway, with the excess far below the window: up.
    EXPECT_TRUE(
        sameDouble((one + powerOfTwo(-53) + powerOfTwo(-400)).toDouble(), 0x1.0000000000001p0));
    // Just below halfway: down.
    EXPECT_TRUE(sameDouble((one + powerOfTwo(-53) - powerOfTwo(-400)).toDouble(), 1.0));
    EXPECT_TRUE(
        sameDouble((-(one + powerOfTwo(-53) + powerOfTwo(-400))).toDouble(), -0x1.0000000000001p0));
    // Subnormals: half the smallest goes to zero, three halves to two.
    EXPECT_TRUE(sameDouble(powerOfTwo(-1075).toDouble(), 0.0));
    EXPECT_TRUE(sameDouble((-powerOfTwo(-1075)).toDouble(), -0.0));
    EXPECT_TRUE(sameDouble((d(3.0) * powerOfTwo(-1075)).toDouble(), 2.0 * kMinSubnormal));
    EXPECT_TRUE(sameDouble((powerOfTwo(-1075) + powerOfTwo(-1300)).toDouble(), kMinSubnormal));
    EXPECT_TRUE(sameDouble((powerOfTwo(-1076) * d(3.0)).toDouble(), kMinSubnormal));
    EXPECT_TRUE(sameDouble(powerOfTwo(-1076).toDouble(), 0.0));
    // The largest subnormal and the smallest normal.
    EXPECT_TRUE(sameDouble((powerOfTwo(-1022) - powerOfTwo(-1076)).toDouble(), 0x1p-1022));
    EXPECT_TRUE(sameDouble((powerOfTwo(-1022) - powerOfTwo(-1075) - powerOfTwo(-1200)).toDouble(),
                           0x1p-1022 - kMinSubnormal));
    // Overflow: halfway between the largest double and 2^1024 rounds to the
    // even one, which is beyond the range.
    EXPECT_TRUE(sameDouble((d(kMaxDouble) + powerOfTwo(970)).toDouble(), kInfinity));
    EXPECT_TRUE(
        sameDouble((d(kMaxDouble) + powerOfTwo(970) - powerOfTwo(900)).toDouble(), kMaxDouble));
    EXPECT_TRUE(sameDouble(powerOfTwo(1024).toDouble(), kInfinity));
    EXPECT_TRUE(sameDouble((-powerOfTwo(5000)).toDouble(), -kInfinity));
    // Integers wider than a double.
    EXPECT_TRUE(sameDouble((powerOfTwo(53) + one).toDouble(), 0x1p53));
    EXPECT_TRUE(sameDouble((powerOfTwo(53) + d(3.0)).toDouble(), 0x1p53 + 4.0));
}

TEST(Dyadic, QuotientMatchesIeeeDivision) {
    Generator generator{4};
    for (int i = 0; i < 20000; ++i) {
        const double x = generator.finite();
        double y = generator.finite();
        if (y == 0.0) {
            y = 1.0;
        }
        const double quotient = x / y;
        ASSERT_TRUE(sameDouble(exact::quotientToDouble(d(x), d(y)), quotient))
            << std::hexfloat << x << ' ' << y;
    }
    for (int i = 0; i < 20000; ++i) {
        const double x = generator.inBinades(-2, 2);
        const double y = generator.inBinades(-2, 2);
        const double quotient = x / y;
        ASSERT_TRUE(sameDouble(exact::quotientToDouble(d(x), d(y)), quotient))
            << std::hexfloat << x << ' ' << y;
    }
}

TEST(Dyadic, QuotientRoundsTiesToEven) {
    const Dyadic three = d(3.0);
    // (2^53 + 1) * 3 / 3 is halfway between 2^53 and 2^53 + 2.
    EXPECT_TRUE(
        sameDouble(exact::quotientToDouble((powerOfTwo(53) + d(1.0)) * three, three), 0x1p53));
    EXPECT_TRUE(sameDouble(exact::quotientToDouble((powerOfTwo(53) + d(3.0)) * three, three),
                           0x1p53 + 4.0));
    EXPECT_TRUE(sameDouble(
        exact::quotientToDouble((powerOfTwo(53) + d(1.0) + powerOfTwo(-300)) * three, three),
        0x1p53 + 2.0));
    // Halfway in the subnormals, and below half the smallest one.
    EXPECT_TRUE(sameDouble(exact::quotientToDouble(d(kMinSubnormal), d(2.0)), 0.0));
    EXPECT_TRUE(
        sameDouble(exact::quotientToDouble(d(3.0 * kMinSubnormal), d(-2.0)), -2.0 * kMinSubnormal));
    EXPECT_TRUE(sameDouble(exact::quotientToDouble(d(kMinSubnormal), d(kMaxDouble)), 0.0));
    EXPECT_TRUE(sameDouble(exact::quotientToDouble(d(-kMaxDouble), d(kMinSubnormal)), -kInfinity));
    // Operands far outside the doubles.
    EXPECT_TRUE(
        sameDouble(exact::quotientToDouble(powerOfTwo(3000) * three, powerOfTwo(3000)), 3.0));
    EXPECT_TRUE(
        sameDouble(exact::quotientToDouble(powerOfTwo(100) + d(1.0), powerOfTwo(100)), 1.0));
    EXPECT_TRUE(sameDouble(exact::quotientToDouble(d(1.0), three), 1.0 / 3.0));
    EXPECT_TRUE(sameDouble(exact::quotientToDouble(d(-0.0), three), -0.0));
    EXPECT_TRUE(sameDouble(exact::quotientToDouble(Dyadic{}, -three), -0.0));
}

TEST(Dyadic, FloorSqrtIsALowerBound) {
    Generator generator{5};
    for (int i = 0; i < 300; ++i) {
        const Dyadic value = d(std::fabs(generator.finite()));
        const Dyadic root = floorSqrt(value, 64);
        // root^2 <= value < (root (1 + 2^-63))^2.
        EXPECT_NE(compare(root * root, value), Sign::positive);
        const Dyadic above = root + root.timesPowerOfTwo(-63);
        EXPECT_EQ(compare(above * above, value), Sign::positive);
    }
    EXPECT_EQ(compare(floorSqrt(d(9.0), 8), d(3.0)), Sign::zero);
    EXPECT_EQ(compare(floorSqrt(d(0x1p-1074), 8), d(0x1p-537)), Sign::zero);
    EXPECT_TRUE(floorSqrt(Dyadic{}, 8).isZero());
}

// --- signOf with one radical --------------------------------------------------

// The double nearest to sqrt(2) = 1.41421356237309504880... is
// 1.4142135623730951454746... (0x1.6a09e667f3bcdp0), above it; its
// predecessor, 1.4142135623730949234300..., is below it.
constexpr double kSqrt2Above = std::numbers::sqrt2;
constexpr double kSqrt2Below = std::bit_cast<double>(std::bit_cast<std::uint64_t>(kSqrt2Above) - 1);
static_assert(std::bit_cast<std::uint64_t>(kSqrt2Above) == 0x3FF6A09E667F3BCDULL);

TEST(SignOfOneRadical, PerfectSquaresAndZeros) {
    EXPECT_EQ(exact::signOf(d(3.0), d(-1.0), d(9.0)), Sign::zero);
    EXPECT_EQ(exact::signOf(d(-3.0), d(1.0), d(9.0)), Sign::zero);
    EXPECT_EQ(exact::signOf(d(-6.0), d(2.0), d(9.0)), Sign::zero);
    EXPECT_EQ(exact::signOf(d(0.5), d(-1.0), d(0.25)), Sign::zero);
    EXPECT_EQ(exact::signOf(d(3.0), d(1.0), d(9.0)), Sign::positive);
    EXPECT_EQ(exact::signOf(d(-3.0), d(-1.0), d(9.0)), Sign::negative);
    // d = 0: the sign of a, whatever b.
    EXPECT_EQ(exact::signOf(d(-1.0), d(1e300), Dyadic{}), Sign::negative);
    EXPECT_EQ(exact::signOf(Dyadic{}, d(1e300), Dyadic{}), Sign::zero);
    // b = 0: the sign of a.
    EXPECT_EQ(exact::signOf(d(2.0), Dyadic{}, d(5.0)), Sign::positive);
    EXPECT_EQ(exact::signOf(Dyadic{}, Dyadic{}, d(5.0)), Sign::zero);
    // a = 0: the sign of b.
    EXPECT_EQ(exact::signOf(Dyadic{}, d(-2.0), d(5.0)), Sign::negative);
    EXPECT_EQ(exact::signOf(Dyadic{}, d(kMinSubnormal), d(kMinSubnormal)), Sign::positive);
}

TEST(SignOfOneRadical, AlmostOppositeTerms) {
    EXPECT_EQ(exact::signOf(d(kSqrt2Above), d(-1.0), d(2.0)), Sign::positive);
    EXPECT_EQ(exact::signOf(d(kSqrt2Below), d(-1.0), d(2.0)), Sign::negative);
    EXPECT_EQ(exact::signOf(d(-kSqrt2Above), d(1.0), d(2.0)), Sign::negative);
    EXPECT_EQ(exact::signOf(d(-kSqrt2Below), d(1.0), d(2.0)), Sign::positive);
    // sqrt(9 + 2^-49) is just above 3, sqrt(9 - 2^-49) just below.
    EXPECT_EQ(exact::signOf(d(3.0), d(-1.0), d(9.0 + 0x1p-49)), Sign::negative);
    EXPECT_EQ(exact::signOf(d(3.0), d(-1.0), d(9.0 - 0x1p-49)), Sign::positive);
    // The same comparisons at the ends of the exponent range: sqrt(2^-1073)
    // is 2^-537 sqrt(2), sqrt(2^1001) is 2^500 sqrt(2).
    EXPECT_EQ(exact::signOf(d(kSqrt2Above * 0x1p-537), d(-1.0), d(0x1p-1073)), Sign::positive);
    EXPECT_EQ(exact::signOf(d(kSqrt2Below * 0x1p-537), d(-1.0), d(0x1p-1073)), Sign::negative);
    EXPECT_EQ(exact::signOf(d(kSqrt2Above * 0x1p500), d(-1.0), d(0x1p1001)), Sign::positive);
    EXPECT_EQ(exact::signOf(d(kSqrt2Below * 0x1p500), d(-1.0), d(0x1p1001)), Sign::negative);
    // A coefficient below the subnormals: 2^-1074 * 2^-1074 sqrt(2) against
    // 2^-2148 * 1.41421356237309514547...
    const Dyadic tiny = d(kMinSubnormal) * d(kMinSubnormal);
    EXPECT_EQ(exact::signOf(tiny * d(kSqrt2Above), -tiny, d(2.0)), Sign::positive);
    EXPECT_EQ(exact::signOf(tiny * d(kSqrt2Below), -tiny, d(2.0)), Sign::negative);
}

// --- signOf with two radicals --------------------------------------------------

QuadraticNumber q(double a, double b, double radicand) {
    return {d(a), d(b), d(radicand)};
}

TEST(SignOfTwoRadicals, ExactZeros) {
    // sqrt(2) + sqrt(8) - 3 sqrt(2) = 0: x = -2 sqrt(2), y = 1, e = 8.
    EXPECT_EQ(exact::signOf(q(0.0, -2.0, 2.0), q(1.0, 0.0, 2.0), d(8.0)), Sign::zero);
    // sqrt(3) + sqrt(12) - 3 sqrt(3) = 0.
    EXPECT_EQ(exact::signOf(q(0.0, -2.0, 3.0), q(1.0, 0.0, 3.0), d(12.0)), Sign::zero);
    // The product radical: sqrt(2) sqrt(8) - 4 = 0.
    EXPECT_EQ(exact::signOf(q(-4.0, 0.0, 2.0), q(0.0, 1.0, 2.0), d(8.0)), Sign::zero);
    // (1 + sqrt(2)) sqrt(8) - 2 sqrt(8) ... written with both radicals:
    // 4 + 2 sqrt(2) - (1 + sqrt(2)) sqrt(8) = 4 + 2 sqrt(2) - 2 sqrt(2) - 4 = 0.
    EXPECT_EQ(exact::signOf(q(4.0, 2.0, 2.0), q(-1.0, -1.0, 2.0), d(8.0)), Sign::zero);
    // (3 - sqrt(2)) sqrt(2) = 3 sqrt(2) - 2: 2 - 3 sqrt(2) + (3 - sqrt(2)) sqrt(2) = 0.
    EXPECT_EQ(exact::signOf(q(2.0, -3.0, 2.0), q(3.0, -1.0, 2.0), d(2.0)), Sign::zero);
}

TEST(SignOfTwoRadicals, OneUlpFromZero) {
    const double above8 = 0x1.0000000000001p3;
    const double below8 = 0x1.fffffffffffffp2;
    // -2 sqrt(2) + sqrt(e), with e one ulp around 8.
    EXPECT_EQ(exact::signOf(q(0.0, -2.0, 2.0), q(1.0, 0.0, 2.0), d(above8)), Sign::positive);
    EXPECT_EQ(exact::signOf(q(0.0, -2.0, 2.0), q(1.0, 0.0, 2.0), d(below8)), Sign::negative);
    // -a + sqrt(2) sqrt(8), with a one ulp around 4.
    EXPECT_EQ(exact::signOf(q(-0x1.0000000000001p2, 0.0, 2.0), q(0.0, 1.0, 2.0), d(8.0)),
              Sign::negative);
    EXPECT_EQ(exact::signOf(q(-0x1.fffffffffffffp1, 0.0, 2.0), q(0.0, 1.0, 2.0), d(8.0)),
              Sign::positive);
    // 4 + 2 sqrt(2) - (1 + sqrt(2)) sqrt(e), e one ulp around 8.
    EXPECT_EQ(exact::signOf(q(4.0, 2.0, 2.0), q(-1.0, -1.0, 2.0), d(above8)), Sign::negative);
    EXPECT_EQ(exact::signOf(q(4.0, 2.0, 2.0), q(-1.0, -1.0, 2.0), d(below8)), Sign::positive);
    // 2 - 3 sqrt(2) + (3 - sqrt(2)) sqrt(e), e one ulp around 2; 3 - sqrt(2) > 0.
    EXPECT_EQ(exact::signOf(q(2.0, -3.0, 2.0), q(3.0, -1.0, 2.0), d(0x1.0000000000001p1)),
              Sign::positive);
    EXPECT_EQ(exact::signOf(q(2.0, -3.0, 2.0), q(3.0, -1.0, 2.0), d(0x1.fffffffffffffp0)),
              Sign::negative);
}

TEST(SignOfTwoRadicals, DecidedBySigns) {
    // Same signs, or a zero term: no squaring needed.
    EXPECT_EQ(exact::signOf(q(1.0, 1.0, 2.0), q(1.0, 1.0, 2.0), d(3.0)), Sign::positive);
    EXPECT_EQ(exact::signOf(q(-1.0, -1.0, 2.0), q(-1.0, 0.0, 2.0), d(3.0)), Sign::negative);
    EXPECT_EQ(exact::signOf(q(-1.0, 0.0, 2.0), q(5.0, 0.0, 2.0), Dyadic{}), Sign::negative);
    EXPECT_EQ(exact::signOf(q(0.0, 0.0, 2.0), q(-1.0, 1.0, 2.0), d(7.0)), Sign::positive);
    EXPECT_EQ(exact::signOf(q(0.0, 0.0, 2.0), q(0.0, 0.0, 2.0), d(7.0)), Sign::zero);
    // x = 1 - sqrt(2) < 0 with y = 0.
    EXPECT_EQ(exact::signOf(q(1.0, -1.0, 2.0), q(0.0, 0.0, 5.0), d(7.0)), Sign::negative);
}

TEST(SignOfTwoRadicals, MatchesSquaredFormForRandomData) {
    // Brute-force check of x + y sqrt(e) through a different route: with x and
    // y of known sign, x + y sqrt(e) has the sign of x when |x| > |y| sqrt(e),
    // that is when x^2 > y^2 e, which here is written with one radical.
    Generator generator{6};
    for (int i = 0; i < 2000; ++i) {
        const QuadraticNumber x = q(generator.inBinades(-4, 4), generator.inBinades(-4, 4),
                                    std::fabs(generator.inBinades(-4, 4)));
        const QuadraticNumber y = q(generator.inBinades(-4, 4), generator.inBinades(-4, 4), 0.0);
        const QuadraticNumber yShared{y.a, y.b, x.d};
        const Dyadic e = d(std::fabs(generator.inBinades(-4, 4)));
        const Sign sx = exact::signOf(x);
        const Sign sy = exact::signOf(yShared);
        const Sign result = exact::signOf(x, yShared, e);
        if (sx == sy) {
            ASSERT_EQ(result, sx);
            continue;
        }
        // |x| vs |y| sqrt(e): x^2 - y^2 e as A + B sqrt(d).
        const Dyadic squares = x.a * x.a + x.b * x.b * x.d - e * (y.a * y.a + y.b * y.b * x.d);
        const Dyadic cross = (x.a * x.b - e * y.a * y.b).timesPowerOfTwo(1);
        const Sign magnitude = exact::signOf(squares, cross, x.d);
        Sign expected = Sign::zero;
        if (magnitude == Sign::positive) {
            expected = sx;
        } else if (magnitude == Sign::negative) {
            expected = sy;
        }
        ASSERT_EQ(result, expected);
    }
}

// --- quadraticToDouble ---------------------------------------------------------

struct Bracket {
    Dyadic low;
    Dyadic high;
};

// An independent square root for the reference: bisection with Dyadic sums,
// halvings and comparisons only, to `steps` bits below the leading one.
Bracket sqrtBracket(const Dyadic& value, int steps) {
    if (value.isZero()) {
        return {Dyadic{}, Dyadic{}};
    }
    Dyadic high = d(1.0);
    while (compare(high * high, value) != Sign::positive) {
        high = high.timesPowerOfTwo(1);
    }
    while (true) {
        const Dyadic half = high.timesPowerOfTwo(-1);
        if (compare(half * half, value) != Sign::positive) {
            break;
        }
        high = half;
    }
    Dyadic low = high.timesPowerOfTwo(-1);
    for (int i = 0; i < steps; ++i) {
        const Dyadic middle = (low + high).timesPowerOfTwo(-1);
        if (compare(middle * middle, value) != Sign::positive) {
            low = middle;
        } else {
            high = middle;
        }
    }
    return {low, high};
}

// Checks that (a + b sqrt(d)) / c lies within 2 ulp of `result`: between the
// double two steps below and the one two steps above.
::testing::AssertionResult withinTwoUlp(double result, const Dyadic& a, const Dyadic& b,
                                        const Dyadic& radicand, const Dyadic& c) {
    // 400 bisection steps: the bracket of the numerator stays far below one ulp
    // of the value even after the deepest cancellation tested here (the Pell
    // case below, about 2^-201 relative to the terms).
    constexpr int kSteps = 400;
    const Bracket root = sqrtBracket(radicand, kSteps);
    const bool bNotNegative = b.sign() != Sign::negative;
    const Dyadic first = a + b * (bNotNegative ? root.low : root.high);
    const Dyadic second = a + b * (bNotNegative ? root.high : root.low);
    // The numerator lies in [first, second].
    const double below = std::nextafter(std::nextafter(result, -kInfinity), -kInfinity);
    const double above = std::nextafter(std::nextafter(result, kInfinity), kInfinity);
    const Dyadic lowest = d(below) * c;
    const Dyadic highest = d(above) * c;
    const bool ok = c.sign() == Sign::positive ? compare(lowest, first) != Sign::positive &&
                                                     compare(second, highest) != Sign::positive
                                               : compare(lowest, second) != Sign::negative &&
                                                     compare(first, highest) != Sign::negative;
    if (ok) {
        return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure()
           << std::hexfloat << result << " for a = " << a.toDouble() << ", b = " << b.toDouble()
           << ", d = " << radicand.toDouble() << ", c = " << c.toDouble();
}

TEST(QuadraticToDouble, ExactCases) {
    EXPECT_TRUE(sameDouble(exact::quadraticToDouble(d(1.0), d(2.0), d(9.0), d(7.0)), 1.0));
    EXPECT_TRUE(sameDouble(exact::quadraticToDouble(d(3.0), d(-1.0), d(9.0), d(5.0)), 0.0));
    EXPECT_TRUE(sameDouble(exact::quadraticToDouble(d(1.0), d(5.0), Dyadic{}, d(-4.0)), -0.25));
    EXPECT_TRUE(sameDouble(exact::quadraticToDouble(d(1.0), Dyadic{}, d(2.0), d(3.0)), 1.0 / 3.0));
    EXPECT_TRUE(
        sameDouble(exact::quadraticToDouble(Dyadic{}, d(1.0), d(0x1p-1074), d(1.0)), 0x1p-537));
    EXPECT_TRUE(sameDouble(exact::quadraticToDouble(Dyadic{}, d(-0.5), d(0x1p-1072), d(0x1p-500)),
                           -0x1p-37));
}

TEST(QuadraticToDouble, WithinTwoUlpOfAHigherPrecisionValue) {
    struct Case {
        double a;
        double b;
        double radicand;
        double c;
    };

    std::vector<Case> cases = {
        {0.0, 1.0, 2.0, 1.0},
        {1.0, 1.0, 2.0, 3.0},
        {kSqrt2Above, -1.0, 2.0, 1.0},  // about 9.7e-17, after cancellation
        {kSqrt2Below, -1.0, 2.0, -1.0},
        {3.0, -1.0, 9.0 + 0x1p-49, 1.0},
        {3.0, -1.0, 9.0 - 0x1p-49, 7.0},
        {1e15, -1e-15, 1e60, 3.0},
        {kMinSubnormal, 0.0, 2.0, 3.0},
        {0.0, kMinSubnormal, 3.0, 1.0},
        {1e-300, 1e-300, 1e-300, 1e300},
        {-1e300, 1e150, 1e300, 1e-10},
    };
    Generator generator{7};
    for (int i = 0; i < 300; ++i) {
        cases.push_back({generator.inBinades(-30, 30), generator.inBinades(-30, 30),
                         std::fabs(generator.inBinades(-60, 60)), generator.inBinades(-30, 30)});
    }
    // Heavy cancellation: a is b sqrt(d) rounded, with the opposite sign.
    for (int i = 0; i < 300; ++i) {
        const double b = generator.inBinades(-20, 20);
        const double radicand = std::fabs(generator.inBinades(-40, 40));
        cases.push_back({-(b * std::sqrt(radicand)), b, radicand, generator.inBinades(-20, 20)});
    }
    for (const Case& item : cases) {
        const double result =
            exact::quadraticToDouble(d(item.a), d(item.b), d(item.radicand), d(item.c));
        EXPECT_TRUE(withinTwoUlp(result, d(item.a), d(item.b), d(item.radicand), d(item.c)));
    }
}

TEST(QuadraticToDouble, CancellationFarBelowTheRootPrecision) {
    // Pell's equation n^2 - (n^2 - 1) * 1^2 = 1: with n = 2^100 + 3,
    // n - sqrt(n^2 - 1) = 1 / (n + sqrt(n^2 - 1)), about 1 / (2n) =
    // 2^-101 (1 - 3 * 2^-100): far closer to 2^-101 than half an ulp, so the
    // nearest double is 2^-101. The two terms agree to about 200 bits, more
    // than any root of fixed precision: only an exact numerator gets this right.
    const Dyadic n = powerOfTwo(100) + d(3.0);
    const Dyadic radicand = n * n - d(1.0);
    EXPECT_TRUE(sameDouble(exact::quadraticToDouble(n, d(-1.0), radicand, d(1.0)), 0x1p-101));
    EXPECT_TRUE(sameDouble(exact::quadraticToDouble(-n, d(1.0), radicand, d(1.0)), -0x1p-101));
    // Divided by 3: the nearest double of 2^-101 / 3, which is (1 / 3) 2^-101
    // because 1 / 3 is nowhere near a tie.
    EXPECT_TRUE(
        sameDouble(exact::quadraticToDouble(n, d(-1.0), radicand, d(3.0)), (1.0 / 3.0) * 0x1p-101));
    EXPECT_TRUE(withinTwoUlp(exact::quadraticToDouble(n, d(-1.0), radicand, d(-5.0)), n, d(-1.0),
                             radicand, d(-5.0)));
    // The same with the terms scaled into the subnormal range of their exponents.
    const Dyadic small = powerOfTwo(-1500);
    EXPECT_TRUE(withinTwoUlp(exact::quadraticToDouble(n * small, -small, radicand, d(7.0)),
                             n * small, -small, radicand, d(7.0)));
}

}  // namespace
