// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// TolerancePolicy (stage 0.2, package P4; ADR 0009). Expected values come from
// IEEE 754 itself (one correctly rounded operation), from hand calculations and
// from the exact sum; the verification cases of R4b are in
// tests/corpus/casi/robust/.

#include "solid_space/robust/tolerance.hpp"

#include "exact_sum.hpp"
#include "solid_space/robust/point.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace robust = solid_space::robust;
using robust::Point2;
using robust::TolerancePolicy;

namespace {

robust::TolerancePolicy policy(double unitInMeters) {
    auto result = TolerancePolicy::forUnit(unitInMeters);
    EXPECT_TRUE(result.hasValue()) << unitInMeters;
    return std::move(result).value();
}

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

    double unit() { return static_cast<double>(next() >> 11U) * 0x1p-53; }

private:
    std::uint64_t m_state;
};

// |b - a|^2 <= epsilon^2, decided by the exact sum alone.
bool exactlyWithin(Point2 a, Point2 b, double epsilon) {
    robust::exact::ExactSum sum;
    sum.add(b.x, b.x);
    sum.subtract(a.x, b.x);
    sum.subtract(a.x, b.x);
    sum.add(a.x, a.x);
    sum.add(b.y, b.y);
    sum.subtract(a.y, b.y);
    sum.subtract(a.y, b.y);
    sum.add(a.y, a.y);
    sum.subtract(epsilon, epsilon);
    return sum.sign() != robust::Sign::positive;
}

}  // namespace

TEST(TolerancePolicy, InvalidUnitsAreDiagnosed) {
    const double inf = std::numeric_limits<double>::infinity();
    const std::array<double, 8> units = {
        0.0,       -0.0, -0.001, inf, -inf, std::numeric_limits<double>::quiet_NaN(),
        1e-30,      // floor 1e21, beyond kMaxCoordinate
        0x1p-1074,  // floor overflows to infinity
    };
    for (const double unit : units) {
        const auto result = TolerancePolicy::forUnit(unit);
        ASSERT_FALSE(result.hasValue()) << unit;
        EXPECT_EQ(result.diagnosis().code, robust::DiagnosisCode::invalidValue);
        EXPECT_FALSE(result.diagnosis().message.empty());
    }
}

TEST(TolerancePolicy, UnitAndFloor) {
    const std::array<double, 6> units = {0.001,  1.0,  1000.0,
                                         0.0254, 1e-9, std::numeric_limits<double>::max()};
    for (const double unit : units) {
        const auto rule = policy(unit);
        EXPECT_EQ(rule.unitInMeters(), unit);
        // One correctly rounded IEEE 754 division.
        EXPECT_EQ(rule.floorLength(), robust::kPhysicalFloorMeters / unit) << unit;
        EXPECT_GT(rule.floorLength(), 0.0);
    }
    EXPECT_EQ(policy(1.0).floorLength(), 1e-9);
    EXPECT_EQ(policy(1e-9).floorLength(), 1.0);
    // The floor may be exactly kMaxCoordinate: 1e-9 / unit rounds to 2^50.
    const double largestFloor = robust::kPhysicalFloorMeters / robust::kMaxCoordinate;
    if (robust::kPhysicalFloorMeters / largestFloor == robust::kMaxCoordinate) {
        EXPECT_EQ(policy(largestFloor).floorLength(), robust::kMaxCoordinate);
    }
}

TEST(TolerancePolicy, Epsilon) {
    const auto rule = policy(0.001);  // floor 1e-6 mm
    const double floor = rule.floorLength();
    EXPECT_EQ(rule.epsilon(0.0), floor);
    EXPECT_EQ(rule.epsilon(20.0), floor);
    // 2^-42 * 2^40 = 0.25, above the floor.
    EXPECT_EQ(rule.epsilon(0x1p40), 0.25);
    EXPECT_EQ(rule.epsilon(robust::kMaxCoordinate), 256.0);
    // Exactly at the switch, M = floor / 2^-42 (exact: a power of two scale).
    const double switchMagnitude = floor * 0x1p42;
    EXPECT_EQ(rule.epsilon(switchMagnitude), floor);
    EXPECT_EQ(rule.epsilon(std::nextafter(switchMagnitude, 0.0)), floor);
    EXPECT_GT(rule.epsilon(std::nextafter(switchMagnitude, 1e300)), floor);
}

TEST(TolerancePolicy, CoincidentAtTheBoundary) {
    const auto rule = policy(1e-9);  // floor exactly 1
    EXPECT_TRUE(rule.coincident({0, 0}, {0, 0}));
    EXPECT_TRUE(rule.coincident({0, 0}, {1, 0}));
    EXPECT_TRUE(rule.coincident({0, 0}, {0, -1}));
    EXPECT_FALSE(rule.coincident({0, 0}, {std::nextafter(1.0, 2.0), 0}));
    // 0.6^2 + 0.8^2 is 1 only up to rounding: the exact sum decides.
    EXPECT_EQ(rule.coincident({0, 0}, {0.6, 0.8}), exactlyWithin({0, 0}, {0.6, 0.8}, 1.0));
    // |(1, 2^-1074)|^2 = 1 + 2^-2148 > 1.
    EXPECT_FALSE(rule.coincident({0, 0}, {1, 0x1p-1074}));
    EXPECT_TRUE(rule.coincident({0, 0}, {std::nextafter(1.0, 0.0), 0x1p-1074}));
    // Relative part: points of magnitude 2^50, epsilon 256.
    const double m = robust::kMaxCoordinate;
    EXPECT_TRUE(rule.coincident({m, m}, {m - 256, m}));
    EXPECT_FALSE(rule.coincident({m, m}, {m - 256, m - 0.25}));
}

TEST(TolerancePolicy, CoincidentIsSymmetricAndAgreesWithTheExactSum) {
    const std::array<double, 4> units = {0.001, 1.0, 1000.0, 1e-9};
    Generator generator(0xc01ceU);
    for (const double unit : units) {
        const auto rule = policy(unit);
        for (int i = 0; i < 4000; ++i) {
            const double scale = std::ldexp(1.0, static_cast<int>(generator.next() % 90U) - 40);
            const Point2 a{(generator.unit() - 0.5) * scale, (generator.unit() - 0.5) * scale};
            // b at a distance close to epsilon, in a random direction.
            const double magnitudeGuess = std::fabs(a.x) + std::fabs(a.y) + scale;
            const double epsilon = rule.epsilon(std::fmin(magnitudeGuess, 0x1p50));
            const double angle = generator.unit();
            const double distance = epsilon * (0.999999 + 2e-6 * generator.unit());
            const Point2 b{
                a.x + distance * (1.0 - 2.0 * angle),
                a.y + distance * std::sqrt(1.0 - (1.0 - 2.0 * angle) * (1.0 - 2.0 * angle))};
            const double m = std::fmax(std::fmax(std::fabs(a.x), std::fabs(a.y)),
                                       std::fmax(std::fabs(b.x), std::fabs(b.y)));
            const bool answer = rule.coincident(a, b);
            ASSERT_EQ(answer, rule.coincident(b, a));
            ASSERT_EQ(answer, exactlyWithin(a, b, rule.epsilon(m)))
                << unit << " " << a.x << " " << a.y << " " << b.x << " " << b.y;
        }
    }
}

TEST(TolerancePolicy, Negligible) {
    const auto rule = policy(0.001);
    const double floor = rule.floorLength();
    EXPECT_TRUE(rule.negligible(0.0, 0.0));
    EXPECT_TRUE(rule.negligible(floor, 0.0));
    EXPECT_FALSE(rule.negligible(std::nextafter(floor, 1.0), 0.0));
    EXPECT_TRUE(rule.negligible(0.25, 0x1p40));
    EXPECT_FALSE(rule.negligible(std::nextafter(0.25, 1.0), 0x1p40));
}
