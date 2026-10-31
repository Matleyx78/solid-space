// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// The private exact sum of robust (stage 0.2, package P4). The expected signs
// come from 64-bit integer arithmetic on small integers, from identities and
// from hand calculations; never from the code under test.

#include "exact_sum.hpp"

#include "solid_space/robust/predicates.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace robust = solid_space::robust;
using robust::Sign;
using robust::exact::ExactSum;

namespace {

constexpr double kMinSubnormal = 0x1p-1074;
constexpr double kMax = std::numeric_limits<double>::max();

// SplitMix64: the same sequence on every platform (the distributions of
// <random> are not portable).
class Generator {
public:
    explicit Generator(std::uint64_t seed) : m_state(seed) {}

    std::uint64_t next() {
        m_state += 0x9e3779b97f4a7c15U;
        std::uint64_t z = m_state;
        z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9U;
        z = (z ^ (z >> 27U)) * 0x94d049bb133111ebU;
        return z ^ (z >> 31U);
    }

    // An integer in (-2^bits, 2^bits).
    std::int64_t integer(unsigned bits) {
        const auto magnitude = static_cast<std::int64_t>(next() >> (64U - bits));
        return (next() & 1U) != 0 ? -magnitude : magnitude;
    }

private:
    std::uint64_t m_state;
};

Sign signOf(std::int64_t value) {
    if (value > 0) {
        return Sign::positive;
    }
    return value < 0 ? Sign::negative : Sign::zero;
}

}  // namespace

TEST(ExactSum, EmptyIsZero) {
    const ExactSum sum;
    EXPECT_EQ(sum.sign(), Sign::zero);
}

TEST(ExactSum, SignOfOneProduct) {
    const std::array<double, 4> values = {3.0, -3.0, 0x1p-1074, -0x1p1023};
    for (const double x : values) {
        for (const double y : values) {
            ExactSum added;
            added.add(x, y);
            ExactSum subtracted;
            subtracted.subtract(x, y);
            const bool positive = (x > 0) == (y > 0);
            EXPECT_EQ(added.sign(), positive ? Sign::positive : Sign::negative) << x << " " << y;
            EXPECT_EQ(subtracted.sign(), positive ? Sign::negative : Sign::positive)
                << x << " " << y;
        }
    }
}

TEST(ExactSum, ZerosOfEitherSignAddNothing) {
    ExactSum sum;
    sum.add(0.0, -5.0);
    sum.add(-0.0, -0.0);
    sum.subtract(-0.0, 7.0);
    sum.add(kMax, 0.0);
    EXPECT_EQ(sum.sign(), Sign::zero);
}

TEST(ExactSum, ProductBelowTheSubnormalsIsNotLost) {
    // 2^-1074 * 2^-1074 = 2^-2148 is zero in doubles, positive here.
    ExactSum sum;
    sum.add(kMinSubnormal, kMinSubnormal);
    EXPECT_EQ(sum.sign(), Sign::positive);
    sum.subtract(kMinSubnormal, kMinSubnormal);
    EXPECT_EQ(sum.sign(), Sign::zero);
}

TEST(ExactSum, TheWholeExponentRangeAtOnce) {
    // The largest product and the smallest one, 2^4096 apart.
    ExactSum largestFirst;
    largestFirst.add(kMax, kMax);
    largestFirst.subtract(kMax, kMax);
    largestFirst.add(kMinSubnormal, kMinSubnormal);
    EXPECT_EQ(largestFirst.sign(), Sign::positive);

    ExactSum smallestFirst;
    smallestFirst.subtract(kMinSubnormal, kMinSubnormal);
    smallestFirst.subtract(-kMax, kMax);
    smallestFirst.add(-kMax, kMax);
    EXPECT_EQ(smallestFirst.sign(), Sign::negative);

    ExactSum largestWins;
    largestWins.subtract(kMinSubnormal, kMinSubnormal);
    largestWins.add(kMax, kMax);
    EXPECT_EQ(largestWins.sign(), Sign::positive);
}

TEST(ExactSum, CarriesCrossEveryLimb) {
    // sum of 2^k for k = 0 .. 199 is 2^200 - 1: adding 1 carries through
    // three limbs and more, then subtracting 2^200 leaves zero. The same at
    // the bottom of the range (unit 2^-2148) and high up (unit 2^1600).
    const std::array<int, 3> bases = {-2148, 0, 1600};
    for (const int base : bases) {
        const double factor = std::ldexp(1.0, base / 2);
        const int rest = base - base / 2;
        ExactSum sum;
        for (int k = 0; k < 200; ++k) {
            sum.add(factor, std::ldexp(1.0, rest + k));
        }
        EXPECT_EQ(sum.sign(), Sign::positive) << base;
        sum.add(factor, std::ldexp(1.0, rest));
        sum.subtract(factor, std::ldexp(1.0, rest + 200));
        EXPECT_EQ(sum.sign(), Sign::zero) << base;
        sum.subtract(factor, std::ldexp(1.0, rest));
        EXPECT_EQ(sum.sign(), Sign::negative) << base;
    }
}

TEST(ExactSum, FullMantissas) {
    // (2^53 - 1)^2 = 2^106 - 2^54 + 1: the product needs all 106 bits.
    const double odd = 0x1.fffffffffffffp52;  // 2^53 - 1
    ExactSum sum;
    sum.add(odd, odd);
    sum.subtract(0x1p53, 0x1p53);
    sum.add(0x1p54, 1.0);
    EXPECT_EQ(sum.sign(), Sign::positive);
    sum.subtract(1.0, 1.0);
    EXPECT_EQ(sum.sign(), Sign::zero);
}

TEST(ExactSum, AgreesWithIntegersAtEveryScale) {
    // Sums of eight products of integers below 2^20, exact in 64 bits, then
    // the same sums with every x scaled by 2^p and every y by 2^q. An integer
    // below 2^53 times 2^p is a double for every p >= -1074 that keeps it
    // finite, also deep in the subnormals, so the sign must not change.
    const std::array<int, 7> scales = {-1074, -1050, -600, -1, 0, 500, 980};
    Generator generator(0x5eedU);
    for (int round = 0; round < 400; ++round) {
        std::array<std::int64_t, 8> x{};
        std::array<std::int64_t, 8> y{};
        std::int64_t expected = 0;
        for (std::size_t i = 0; i < x.size(); ++i) {
            x.at(i) = generator.integer(20);
            y.at(i) = generator.integer(20);
            expected += (i % 2 == 0 ? 1 : -1) * x.at(i) * y.at(i);
        }
        // Every other round, force a zero sum by cancelling the last term.
        if (round % 2 == 1) {
            const std::int64_t rest = expected + x.back() * y.back();  // i = 7 is subtracted
            x.back() = rest;
            y.back() = 1;
            expected = 0;
        }
        for (const int p : scales) {
            for (const int q : scales) {
                ExactSum sum;
                for (std::size_t i = 0; i < x.size(); ++i) {
                    const double xi = std::ldexp(static_cast<double>(x.at(i)), p);
                    const double yi = std::ldexp(static_cast<double>(y.at(i)), q);
                    if (i % 2 == 0) {
                        sum.add(xi, yi);
                    } else {
                        sum.subtract(xi, yi);
                    }
                }
                ASSERT_EQ(sum.sign(), signOf(expected)) << round << " " << p << " " << q;
            }
        }
    }
}
