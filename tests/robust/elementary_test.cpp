// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "solid_space/robust/elementary.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <ios>
#include <limits>
#include <numbers>
#include <random>

namespace robust = solid_space::robust;

namespace {

constexpr double kPi = std::numbers::pi;
constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr int kSamples = 20000;

std::uint64_t bits(double value) {
    return std::bit_cast<std::uint64_t>(value);
}

// The engine sequence and the conversion to a fraction are platform independent.
double unit(std::mt19937_64& random) {
    return static_cast<double>(random() >> 11U) * 0x1p-53;
}

void expectNearLibm(double actual, double reference) {
    // The reference here is a platform library result. Allow a few ulps for
    // its error as well as ours. The corpus checks the contractual bound.
    const double magnitude = std::fabs(reference);
    const double ulp = std::nextafter(magnitude, kInfinity) - magnitude;
    EXPECT_LE(std::fabs(actual - reference), 4.0 * ulp);
}

void checkTangent(double angle) {
    const double result = robust::tan(angle);
    EXPECT_LT(std::fabs(result), 0x1p61) << std::hexfloat << angle;
    EXPECT_EQ(bits(robust::tan(-angle)), bits(-result)) << std::hexfloat << angle;
    expectNearLibm(result, std::tan(angle));
}

void checkArccosine(double x) {
    const double result = robust::acos(x);
    EXPECT_FALSE(std::signbit(result));
    EXPECT_LE(result, kPi);
    expectNearLibm(result, std::acos(x));
}

void checkLength(double x, double y) {
    const double result = robust::hypot(x, y);
    EXPECT_FALSE(std::signbit(result));
    EXPECT_GE(result, std::max(std::fabs(x), std::fabs(y)));
    EXPECT_EQ(bits(result), bits(robust::hypot(y, x)));
    EXPECT_EQ(bits(result), bits(robust::hypot(-x, y)));
    EXPECT_EQ(bits(result), bits(robust::hypot(x, -y)));
    EXPECT_EQ(result == 0.0, x == 0.0 && y == 0.0);
    if (result != 0.0) {
        EXPECT_LE(std::fabs(x / result), 1.0);
        EXPECT_LE(std::fabs(y / result), 1.0);
    }
    expectNearLibm(result, std::hypot(x, y));
}

}  // namespace

TEST(Elementary, TangentIsOddAndFinite) {
    EXPECT_EQ(bits(robust::tan(0.0)), bits(0.0));
    EXPECT_EQ(bits(robust::tan(-0.0)), bits(-0.0));
    for (const double angle : {0x1p-1074, 0x1p-27, robust::kMaxTrigArgument}) {
        checkTangent(angle);
    }
    std::mt19937_64 random(0x74616eU);
    for (int i = 0; i < kSamples; ++i) {
        checkTangent(unit(random) * robust::kMaxTrigArgument);
        // Exercise both sides of a pole and all the odd quadrants.
        const double pole = (2 * i + 1) * (kPi / 2.0);
        checkTangent(std::nextafter(pole, 0.0));
        checkTangent(pole);
        checkTangent(std::nextafter(pole, kInfinity));
    }
}

TEST(Elementary, ArccosineStaysInRangeAndHasExactEndpoints) {
    EXPECT_EQ(bits(robust::acos(1.0)), bits(0.0));
    EXPECT_EQ(bits(robust::acos(-1.0)), bits(kPi));
    checkArccosine(0.0);
    checkArccosine(-0.0);
    std::mt19937_64 random(0x61636f73U);
    double nearOne = 1.0;
    for (int i = 0; i < kSamples; ++i) {
        nearOne = std::nextafter(nearOne, 0.0);
        checkArccosine(nearOne);
        checkArccosine(-nearOne);
        checkArccosine(2.0 * unit(random) - 1.0);
    }
}

TEST(Elementary, LengthIsSymmetricAndBoundsTheComponents) {
    checkLength(-0.0, -0.0);
    checkLength(robust::kMaxHypotArgument, robust::kMaxHypotArgument);
    checkLength(robust::kMaxHypotArgument, 0x1p-1074);
    std::mt19937_64 random(0x6879706f74U);
    // Exercise nearby magnitudes and both sides of the exponent-gap shortcut.
    for (int i = 0; i < kSamples; ++i) {
        constexpr int kMinExponent = -1074;
        constexpr int kExponentCount = 2097;
        const int exponent = kMinExponent + static_cast<int>(random() % kExponentCount);
        const double x = std::ldexp(1.0 + unit(random), exponent);
        const int gap = static_cast<int>(random() % 30U);
        const double y = std::ldexp(1.0 + unit(random), exponent - gap);
        checkLength(x, y);
    }
}

TEST(Elementary, LengthReturnsExactRepresentableTriples) {
    // Walk from the smallest subnormal scale to the upper component limit.
    for (int exponent = -1074; exponent <= 1021; ++exponent) {
        EXPECT_EQ(bits(robust::hypot(std::ldexp(3.0, exponent), std::ldexp(4.0, exponent))),
                  bits(std::ldexp(5.0, exponent)));
        if (exponent <= 1019) {
            EXPECT_EQ(bits(robust::hypot(std::ldexp(5.0, exponent), std::ldexp(12.0, exponent))),
                      bits(std::ldexp(13.0, exponent)));
        }
    }
}
