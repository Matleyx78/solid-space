// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// sinCos and atan2 (stage 0.2, package P5; ADR 0010). These tests check
// identities, symmetries, continuity and ranges, not values: the expected
// values with their tolerances are the verification cases of R5a and R5b in
// tests/corpus/casi/robust/. The only constants here are the double nearest
// to pi (std::numbers::pi) and the C standard's signed zeros (issue #31).

#include "solid_space/robust/trigonometry.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>

namespace robust = solid_space::robust;

namespace {

constexpr double kPi = std::numbers::pi;
constexpr double kInfinity = std::numeric_limits<double>::infinity();

// SplitMix64: the same sequence on every platform.
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

    // A double in [0, 1) with 53 random bits.
    double unit() { return static_cast<double>(next() >> 11U) * 0x1p-53; }

    // A double in (-limit, limit).
    double symmetric(double limit) {
        const double magnitude = unit() * limit;
        return (next() & 1U) != 0 ? -magnitude : magnitude;
    }

    // A double of random sign in [2^e, 2^(e + 1)), e uniform in [low, high).
    double logarithmic(int low, int high) {
        const auto span = static_cast<std::uint64_t>(high - low);
        const int exponent = low + static_cast<int>(next() % span);
        const double magnitude = std::ldexp(1.0 + unit(), exponent);
        return (next() & 1U) != 0 ? -magnitude : magnitude;
    }

private:
    std::uint64_t m_state;
};

std::uint64_t bits(double value) {
    return std::bit_cast<std::uint64_t>(value);
}

double ulp(double value) {
    const double magnitude = std::fabs(value);
    return std::nextafter(magnitude, kInfinity) - magnitude;
}

// Angles for sinCos: a fine grid on [-4 pi, 4 pi], a coarse one up to 2^20,
// random angles of every size, and the limits.
std::vector<double> angles() {
    std::vector<double> result;
    for (int i = -40000; i <= 40000; ++i) {
        result.push_back(i * (kPi / 10000.0));
    }
    for (int i = 0; i < 4096; ++i) {
        result.push_back(i * 256.000390625);  // up to 2^20 - 254
    }
    Generator generator(0x7269676f6e6f6dU);
    for (int i = 0; i < 20000; ++i) {
        result.push_back(generator.symmetric(robust::kMaxTrigArgument));
        result.push_back(generator.logarithmic(-60, 20));
    }
    for (const double limit :
         {robust::kMaxTrigArgument, std::nextafter(robust::kMaxTrigArgument, 0.0), 0x1p-1074,
          0x1p-1022, 0x1p-27, std::nextafter(0x1p-27, 0.0)}) {
        result.push_back(limit);
        result.push_back(-limit);
    }
    return result;
}

// Vectors for atan2: every quadrant, ratios from 2^-60 to 2^60, magnitudes
// from the subnormals to 2^1000.
std::vector<std::pair<double, double>> vectors() {
    std::vector<std::pair<double, double>> result;
    Generator generator(0x6174616e32U);
    for (int i = 0; i < 40000; ++i) {
        const double x = generator.logarithmic(-60, 60);
        result.emplace_back(generator.logarithmic(-60, 60) * std::fabs(x), x);
        result.emplace_back(generator.symmetric(1.0), generator.symmetric(1.0));
    }
    for (int i = 0; i < 2000; ++i) {
        const double scale = generator.logarithmic(-1074, 1000);
        result.emplace_back(generator.symmetric(1.0) * std::fabs(scale),
                            generator.symmetric(1.0) * std::fabs(scale));
    }
    return result;
}

}  // namespace

TEST(Trigonometry, SineAndCosineSquaredAddToOne) {
    // Each value is within kTrigMaxErrorUlps of the exact one, relative error
    // below 2^-51 each: s^2 + c^2 is within 2^-50 of 1, with the two roundings
    // of the check.
    for (const double angle : angles()) {
        const auto [sine, cosine] = robust::sinCos(angle);
        const double sum = std::fma(sine, sine, cosine * cosine);
        EXPECT_LE(std::fabs(sum - 1.0), 0x1p-50) << std::hexfloat << angle;
    }
}

TEST(Trigonometry, SineAndCosineStayInUnitRange) {
    std::vector<double> near = angles();
    for (int k = -64; k <= 64; ++k) {  // where sine or cosine is about +-1
        double angle = k * (kPi / 2.0);
        for (int step = 0; step < 32; ++step) {
            near.push_back(angle);
            angle = std::nextafter(angle, kInfinity);
        }
    }
    for (const double angle : near) {
        const auto [sine, cosine] = robust::sinCos(angle);
        EXPECT_LE(std::fabs(sine), 1.0) << std::hexfloat << angle;
        EXPECT_LE(std::fabs(cosine), 1.0) << std::hexfloat << angle;
    }
}

TEST(Trigonometry, SinCosIsOddAndEvenBitForBit) {
    for (const double angle : angles()) {
        const auto positive = robust::sinCos(angle);
        const auto negative = robust::sinCos(-angle);
        EXPECT_EQ(bits(negative.sine), bits(-positive.sine)) << std::hexfloat << angle;
        EXPECT_EQ(bits(negative.cosine), bits(positive.cosine)) << std::hexfloat << angle;
    }
    const auto zero = robust::sinCos(-0.0);
    EXPECT_TRUE(std::signbit(zero.sine));
    EXPECT_EQ(zero.cosine, 1.0);
}

TEST(Trigonometry, SinCosIsContinuousAtTheReductionBorders) {
    // The quadrant changes near the odd multiples of pi/4, and the small-angle
    // rule at 2^-27. Walking one ulp at a time across each border, a value
    // moves by at most the step (|derivative| <= 1) plus the error of the two
    // values, below 2 * 2^-52 each.
    std::vector<double> borders = {0x1p-27, -0x1p-27};
    for (const double multiple : {1.0, 3.0, 5.0, 7.0, 101.0, 4097.0, 1335003.0}) {
        borders.push_back(multiple * (kPi / 4.0));
        borders.push_back(-multiple * (kPi / 4.0));
    }
    for (const double border : borders) {
        double angle = border;
        for (int step = 0; step < 64; ++step) {
            angle = std::nextafter(angle, -kInfinity);
        }
        auto previous = robust::sinCos(angle);
        for (int step = 0; step < 128; ++step) {
            const double next = std::nextafter(angle, kInfinity);
            const auto current = robust::sinCos(next);
            const double bound = (next - angle) + 0x1p-50;
            EXPECT_LE(std::fabs(current.sine - previous.sine), bound) << std::hexfloat << next;
            EXPECT_LE(std::fabs(current.cosine - previous.cosine), bound) << std::hexfloat << next;
            angle = next;
            previous = current;
        }
    }
}

TEST(Trigonometry, Atan2StaysInRange) {
    for (const auto& [y, x] : vectors()) {
        const double angle = robust::atan2(y, x);
        EXPECT_LE(std::fabs(angle), kPi) << std::hexfloat << y << ' ' << x;
    }
    // Just above and below the negative x axis, where the angle is about +-pi.
    for (const double y : {0x1p-1074, 0x1p-60, 0x1p-30}) {
        for (const double x : {-1.0, -0x1p30, -0x1p1000}) {
            EXPECT_LE(robust::atan2(y, x), kPi) << std::hexfloat << y << ' ' << x;
            EXPECT_GE(robust::atan2(-y, x), -kPi) << std::hexfloat << y << ' ' << x;
        }
    }
}

TEST(Trigonometry, Atan2IsOddInYBitForBit) {
    std::vector<std::pair<double, double>> all = vectors();
    for (const double x : {1.0, -1.0, 0.0, -0.0, 0x1p-1074, -0x1p-1074}) {
        for (const double y : {0.0, 1.0, 0x1p-1074, 0x1p1000}) {
            all.emplace_back(y, x);
        }
    }
    for (const auto& [y, x] : all) {
        EXPECT_EQ(bits(robust::atan2(-y, x)), bits(-robust::atan2(y, x)))
            << std::hexfloat << y << ' ' << x;
    }
}

TEST(Trigonometry, Atan2SignedZerosFollowTheCStandard) {
    // Issue #31: atan2(+-0, +0) is +-0 and atan2(+-0, -0) is +-pi.
    EXPECT_EQ(bits(robust::atan2(0.0, 0.0)), bits(0.0));
    EXPECT_EQ(bits(robust::atan2(-0.0, 0.0)), bits(-0.0));
    EXPECT_EQ(robust::atan2(0.0, -0.0), kPi);
    EXPECT_EQ(robust::atan2(-0.0, -0.0), -kPi);
    EXPECT_EQ(bits(robust::atan2(-0.0, 1.0)), bits(-0.0));
    EXPECT_EQ(robust::atan2(0.0, -1.0), kPi);
    EXPECT_EQ(robust::atan2(-0.0, -1.0), -kPi);
}

TEST(Trigonometry, Atan2IsContinuousAtTheReductionBorders) {
    // atan(y / x) for x = 1 changes reduction centre at y = (2j + 1) / 16,
    // switches to pi/2 - atan(x / y) at y = 1 and uses the small-ratio rule
    // below 2^-27. Walking one ulp at a time, the angle moves by at most the
    // step plus the error of the two values.
    std::vector<double> borders = {0x1p-27, 1.0};
    for (int j = 0; j < 8; ++j) {
        borders.push_back((2.0 * j + 1.0) / 16.0);
    }
    for (const double border : borders) {
        for (const double x : {1.0, -1.0}) {
            double y = border;
            for (int step = 0; step < 64; ++step) {
                y = std::nextafter(y, -kInfinity);
            }
            double previous = robust::atan2(y, x);
            for (int step = 0; step < 128; ++step) {
                const double next = std::nextafter(y, kInfinity);
                const double current = robust::atan2(next, x);
                EXPECT_LE(std::fabs(current - previous), (next - y) + 0x1p-50)
                    << std::hexfloat << next << ' ' << x;
                y = next;
                previous = current;
            }
        }
    }
}

TEST(Trigonometry, Atan2InvertsSinCos) {
    // For a in [-pi, pi], atan2(sin a, cos a) is a: the errors of sine and
    // cosine (about 1 ulp each, relative) and of atan2 (about 1 ulp) leave it
    // within a few ulp of a.
    for (int i = -40000; i <= 40000; ++i) {
        const double angle = i * (kPi / 40001.0);
        const auto [sine, cosine] = robust::sinCos(angle);
        const double back = robust::atan2(sine, cosine);
        EXPECT_LE(std::fabs(back - angle), 6.0 * ulp(angle) + 0x1p-1070) << std::hexfloat << angle;
    }
}
