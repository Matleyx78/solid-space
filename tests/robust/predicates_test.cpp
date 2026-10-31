// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// orient2d (stage 0.2, package P4): the filter and the exact sum agree, the
// sign follows the parity of a permutation, and scaling by a power of two does
// not change it. Expected signs come from hand calculations and identities;
// the verification cases of R4a are in tests/corpus/casi/robust/.

#include "solid_space/robust/predicates.hpp"

#include "exact_sum.hpp"
#include "solid_space/robust/point.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace robust = solid_space::robust;
using robust::Point2;
using robust::Sign;

namespace {

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

    // An integer in (-2^bits, 2^bits).
    double integer(unsigned bits) {
        const auto magnitude = static_cast<double>(next() >> (64U - bits));
        return (next() & 1U) != 0 ? -magnitude : magnitude;
    }

private:
    std::uint64_t m_state;
};

Sign negate(Sign sign) {
    return static_cast<Sign>(-static_cast<int>(sign));
}

// The exact sign with no filter: (bx - ax)(cy - ay) - (by - ay)(cx - ax)
// developed into products of the given doubles.
Sign exactOrientation(Point2 a, Point2 b, Point2 c) {
    robust::exact::ExactSum sum;
    sum.add(b.x, c.y);
    sum.subtract(b.x, a.y);
    sum.subtract(a.x, c.y);
    sum.subtract(b.y, c.x);
    sum.add(b.y, a.x);
    sum.add(a.y, c.x);
    return sum.sign();
}

// Triples of every kind: random, nearly collinear (a point on a segment,
// rounded and then moved by up to two ulps), exactly collinear on integer
// lines, with coincident points, at very different scales.
std::vector<std::array<Point2, 3>> triples() {
    std::vector<std::array<Point2, 3>> result;
    Generator generator(0x0412d0U);
    for (int i = 0; i < 2000; ++i) {
        const double scale = std::ldexp(1.0, static_cast<int>(generator.next() % 80U) - 40);
        const Point2 a{(generator.unit() - 0.5) * scale, (generator.unit() - 0.5) * scale};
        const Point2 b{(generator.unit() - 0.5) * scale, (generator.unit() - 0.5) * scale};
        const double t = generator.unit();
        Point2 c{a.x + t * (b.x - a.x), a.y + t * (b.y - a.y)};
        const int steps = static_cast<int>(generator.next() % 5U) - 2;
        for (int s = 0; s < std::abs(steps); ++s) {
            c.x = std::nextafter(c.x, steps > 0 ? 1e300 : -1e300);
        }
        result.push_back({a, b, c});
        const Point2 d{(generator.unit() - 0.5) * scale, (generator.unit() - 0.5) * scale};
        result.push_back({a, b, d});
    }
    for (int i = 0; i < 500; ++i) {
        const Point2 origin{generator.integer(20), generator.integer(20)};
        const Point2 direction{generator.integer(10), generator.integer(10)};
        const double m = generator.integer(10);
        const double n = generator.integer(10);
        const Point2 b{origin.x + m * direction.x, origin.y + m * direction.y};
        const Point2 c{origin.x + n * direction.x, origin.y + n * direction.y};
        result.push_back({origin, b, c});
        result.push_back({origin, b, Point2{c.x + 1, c.y}});
        result.push_back({origin, origin, c});
    }
    return result;
}

}  // namespace

TEST(Orient2d, HandCases) {
    const Point2 o{0, 0};
    EXPECT_EQ(robust::orient2d(o, {1, 0}, {0, 1}), Sign::positive);
    EXPECT_EQ(robust::orient2d(o, {0, 1}, {1, 0}), Sign::negative);
    EXPECT_EQ(robust::orient2d(o, {1, 1}, {2, 2}), Sign::zero);
    EXPECT_EQ(robust::orient2d({3, 4}, {3, 4}, {7, -1}), Sign::zero);
    EXPECT_EQ(robust::orient2d({-0.0, 0.0}, {1, 1}, {2, 2}), Sign::zero);
    // (1 - 0) * (0.5 - 0) - (1 - 0) * (0.5 + 2^-53 - 0) = -2^-53.
    EXPECT_EQ(robust::orient2d(o, {1, 1}, {std::nextafter(0.5, 1.0), 0.5}), Sign::negative);
    EXPECT_EQ(robust::orient2d(o, {1, 1}, {0.5, std::nextafter(0.5, 1.0)}), Sign::positive);
}

TEST(Orient2d, ProductsBelowTheSubnormals) {
    // The example of the package: 1e-200 * 1e-200 is zero in doubles.
    EXPECT_EQ(robust::orient2d({0, 0}, {1e-200, 0}, {0, 1e-200}), Sign::positive);
    EXPECT_EQ(robust::orient2d({0, 0}, {0, 1e-200}, {1e-200, 0}), Sign::negative);
    const double tiny = 0x1p-1074;
    EXPECT_EQ(robust::orient2d({0, 0}, {tiny, 0}, {0, tiny}), Sign::positive);
    EXPECT_EQ(robust::orient2d({tiny, tiny}, {2 * tiny, 2 * tiny}, {3 * tiny, 3 * tiny}),
              Sign::zero);
}

TEST(Orient2d, LargestCoordinates) {
    const double m = robust::kMaxCoordinate;
    EXPECT_EQ(robust::orient2d({-m, -m}, {m, -m}, {-m, m}), Sign::positive);
    EXPECT_EQ(robust::orient2d({-m, -m}, {m, m}, {0, 0}), Sign::zero);
    // One unit below the diagonal, with a determinant of -2^51 next to
    // products of 2^101.
    EXPECT_EQ(robust::orient2d({-m, -m}, {m, m}, {1, 0}), Sign::negative);
}

TEST(Orient2d, FilterAndExactSumAgree) {
    for (const auto& [a, b, c] : triples()) {
        ASSERT_EQ(robust::orient2d(a, b, c), exactOrientation(a, b, c))
            << a.x << " " << a.y << " " << b.x << " " << b.y << " " << c.x << " " << c.y;
    }
}

TEST(Orient2d, PermutationsFollowTheirParity) {
    for (const auto& [a, b, c] : triples()) {
        const Sign sign = robust::orient2d(a, b, c);
        ASSERT_EQ(robust::orient2d(b, c, a), sign);
        ASSERT_EQ(robust::orient2d(c, a, b), sign);
        ASSERT_EQ(robust::orient2d(b, a, c), negate(sign));
        ASSERT_EQ(robust::orient2d(a, c, b), negate(sign));
        ASSERT_EQ(robust::orient2d(c, b, a), negate(sign));
    }
}

TEST(Orient2d, ScalingByAPowerOfTwoKeepsTheSign) {
    // Integer coordinates below 2^20 stay exact when scaled by 2^k for every
    // k from -1074 to 30, deep in the subnormals and within kMaxCoordinate.
    const std::array<int, 8> scales = {-1074, -1060, -1040, -1000, -540, -1, 1, 30};
    Generator generator(0x5ca1eU);
    for (int i = 0; i < 1000; ++i) {
        std::array<Point2, 3> points{};
        for (auto& point : points) {
            point = {generator.integer(20), generator.integer(20)};
        }
        if (i % 3 == 0) {
            // Exactly collinear: c = a + (b - a) * 2, with small integers.
            points[0] = {generator.integer(10), generator.integer(10)};
            points[1] = {generator.integer(10), generator.integer(10)};
            points[2] = {2 * points[1].x - points[0].x, 2 * points[1].y - points[0].y};
        }
        const Sign sign = robust::orient2d(points[0], points[1], points[2]);
        for (const int k : scales) {
            std::array<Point2, 3> scaled{};
            for (std::size_t j = 0; j < points.size(); ++j) {
                scaled.at(j) = {std::ldexp(points.at(j).x, k), std::ldexp(points.at(j).y, k)};
                ASSERT_EQ(std::ldexp(scaled.at(j).x, -k), points.at(j).x);
                ASSERT_EQ(std::ldexp(scaled.at(j).y, -k), points.at(j).y);
            }
            ASSERT_EQ(robust::orient2d(scaled[0], scaled[1], scaled[2]), sign) << i << " " << k;
        }
    }
}
