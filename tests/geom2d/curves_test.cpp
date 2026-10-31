// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Unit tests of pieces, magnitude, endpoints and length (stage 0.2, package
// P7). The ends of the arcs are checked against their exact meaning with the
// exact numbers of geom2d: each rounded coordinate p is within the declared
// error E of c + r * d / |d|, which is decided exactly as the sign of a number
// a + b * sqrt(|d|^2). The expected values never come from running the code
// under test.

#include "solid_space/geom2d/curves.hpp"

#include "curve_meaning.hpp"
#include "exact/quadratic.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <variant>

namespace {

using solid_space::geom2d::Arc;
using solid_space::geom2d::Circle;
using solid_space::geom2d::DiagnosisCode;
using solid_space::geom2d::kFullTurn;
using solid_space::geom2d::kRelativePointError;
using solid_space::geom2d::kRelativeStationError;
using solid_space::geom2d::Point2;
using solid_space::geom2d::Polyline;
using solid_space::geom2d::PolylineSegment;
using solid_space::geom2d::Segment;
using solid_space::geom2d::exact::Dyadic;
using solid_space::robust::Sign;

namespace geom2d = solid_space::geom2d;
namespace detail = solid_space::geom2d::detail;
namespace exact = solid_space::geom2d::exact;

// 2 pi - kFullTurn, the part of 2 pi that the double does not hold: about
// 2.449e-16, from the digits of pi (6.28318530717958647692528676655...).
constexpr double kFullTurnRemainder = 2.4492935982947064e-16;

// SplitMix64: a deterministic stream, the same on every platform.
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

    // A double in [-limit, limit), with 53 random bits.
    double uniform(double limit) {
        const double unit = static_cast<double>(next() >> 11U) * 0x1p-53;
        return (2.0 * unit - 1.0) * limit;
    }

private:
    std::uint64_t m_state;
};

// True when |p - (c + r * d / sqrt(n))| <= error, with n = |direction|^2,
// decided exactly: (p - error - c) sqrt(n) - r d <= 0 and
// (c - p - error) sqrt(n) + r d <= 0.
bool withinOfExact(double p, double c, double r, const Dyadic& d, const Dyadic& n, double error) {
    const Dyadic rd = Dyadic(r) * d;
    const Dyadic low = Dyadic(p) - Dyadic(error) - Dyadic(c);
    const Dyadic high = Dyadic(c) - Dyadic(p) - Dyadic(error);
    return exact::signOf(-rd, low, n) != Sign::positive &&
           exact::signOf(rd, high, n) != Sign::positive;
}

void expectPointInDirection(Point2 point, const Arc& arc, const detail::ExactVector& direction,
                            double magnitude) {
    const Dyadic n = direction.x * direction.x + direction.y * direction.y;
    const double size = std::max({magnitude, std::fabs(point.x), std::fabs(point.y)});
    // A little more than the bound with |p| of the exact point, which differs
    // from |p| of the rounded one by far less than 2^-46 * size.
    const double error = kRelativePointError * size * (1.0 + 0x1p-40);
    EXPECT_TRUE(withinOfExact(point.x, arc.center.x, arc.radius, direction.x, n, error));
    EXPECT_TRUE(withinOfExact(point.y, arc.center.y, arc.radius, direction.y, n, error));
}

// Centres and radii far from the origin, near it, and small.
constexpr std::array<double, 3> kScales = {1e6, 1.0, 1e-3};

TEST(Endpoints, ArcEndsAgreeWithTheExactMeaning) {
    Generator generator(0xA4C);
    int checked = 0;
    for (int i = 0; i < 3000; ++i) {
        const double scale = kScales.at(static_cast<std::size_t>(i) % kScales.size());
        const Arc arc{{generator.uniform(scale), generator.uniform(scale)},
                      std::fabs(generator.uniform(scale)),
                      generator.uniform(10.0),
                      generator.uniform(8.0)};
        const auto meaning = detail::checkedMeaning(arc, 0);
        if (!meaning.hasValue()) {
            continue;
        }
        const auto& exactArc = std::get<detail::ArcMeaning>(meaning.value().support);
        const auto ends = geom2d::endpoints(arc);
        ASSERT_TRUE(ends.hasValue());
        const double magnitude = meaning.value().magnitude;
        expectPointInDirection(ends.value().start, arc, exactArc.startDirection, magnitude);
        // A whole circle ends in direction D0, the others in direction D1.
        expectPointInDirection(ends.value().end, arc, exactArc.endDirection, magnitude);
        if (exactArc.span == detail::ArcSpan::whole) {
            EXPECT_EQ(ends.value().start.x, ends.value().end.x);
            EXPECT_EQ(ends.value().start.y, ends.value().end.y);
        }
        ++checked;
    }
    EXPECT_GT(checked, 2900);
}

TEST(Endpoints, OtherCurvesEndAtTheirData) {
    const auto segment = geom2d::endpoints(Segment{{1.0, 2.0}, {-3.0, 0.5}});
    ASSERT_TRUE(segment.hasValue());
    EXPECT_EQ(segment.value().start.x, 1.0);
    EXPECT_EQ(segment.value().end.y, 0.5);
    const auto piece = geom2d::endpoints(PolylineSegment{{0.1, 0.2}, {0.3, 0.7}, -3.0});
    ASSERT_TRUE(piece.hasValue());
    EXPECT_EQ(piece.value().start.x, 0.1);
    EXPECT_EQ(piece.value().end.y, 0.7);
    // A circle starts and ends at angle zero.
    const auto circle = geom2d::endpoints(Circle{{1e6, -2.0}, 0.25});
    ASSERT_TRUE(circle.hasValue());
    EXPECT_EQ(circle.value().start.x, 1e6 + 0.25);
    EXPECT_EQ(circle.value().start.y, -2.0);
    EXPECT_EQ(circle.value().end.x, 1e6 + 0.25);
}

TEST(Length, WholeCircleWithinTheErrorOf2PiR) {
    Generator generator(0x2B1);
    for (int i = 0; i < 1000; ++i) {
        const double radius = std::ldexp(std::fabs(generator.uniform(1.0)) + 0.5, (i % 91) - 50);
        // 2 pi r to about 2^-105 relative: the error of this reference is far
        // below the bound.
        const double exactLength = kFullTurn * radius + kFullTurnRemainder * radius;
        const double bound = kRelativeStationError * exactLength;
        const auto circle = geom2d::length(Circle{{radius, -radius}, radius});
        ASSERT_TRUE(circle.hasValue());
        EXPECT_LE(std::fabs(circle.value() - exactLength), bound);
        const auto whole = geom2d::length(Arc{{0.0, 0.0}, radius, 1.0, -8.0});
        ASSERT_TRUE(whole.hasValue());
        EXPECT_LE(std::fabs(whole.value() - exactLength), bound);
    }
}

TEST(Length, SegmentsAndPieces) {
    const auto segment = geom2d::length(Segment{{1.0, 1.0}, {4.0, 5.0}});
    ASSERT_TRUE(segment.hasValue());
    EXPECT_EQ(segment.value(), 5.0);
    const auto straight = geom2d::length(PolylineSegment{{1.0, 1.0}, {-2.0, -3.0}, -0.0});
    ASSERT_TRUE(straight.hasValue());
    EXPECT_EQ(straight.value(), 5.0);
    // bulge -1 over a chord of 2: half the circle of radius 1, clockwise.
    const auto half = geom2d::length(PolylineSegment{{0.0, 0.0}, {2.0, 0.0}, -1.0});
    ASSERT_TRUE(half.hasValue());
    EXPECT_LE(std::fabs(half.value() - (kFullTurn / 2.0 + kFullTurnRemainder / 2.0)),
              kRelativeStationError * 2.0);
    // A tiny bulge: the length is the chord.
    const auto flat = geom2d::length(PolylineSegment{{0.0, 0.0}, {3.0, 4.0}, 1e-300});
    ASSERT_TRUE(flat.hasValue());
    EXPECT_EQ(flat.value(), 5.0);
}

TEST(Pieces, OpenAndClosedPolylines) {
    Polyline polyline{{{{0.0, 0.0}, 0.5}, {{1.0, 0.0}, 0.0}, {{1.0, 1.0}, -0.25}}, false};
    const auto open = geom2d::pieces(polyline);
    ASSERT_EQ(open.size(), 2U);
    EXPECT_EQ(open[0].bulge, 0.5);
    EXPECT_EQ(open[1].start.x, 1.0);
    EXPECT_EQ(open[1].end.y, 1.0);
    polyline.closed = true;
    const auto closed = geom2d::pieces(polyline);
    ASSERT_EQ(closed.size(), 3U);
    // The last piece returns to the first vertex with the last bulge.
    EXPECT_EQ(closed[2].start.x, 1.0);
    EXPECT_EQ(closed[2].start.y, 1.0);
    EXPECT_EQ(closed[2].end.x, 0.0);
    EXPECT_EQ(closed[2].end.y, 0.0);
    EXPECT_EQ(closed[2].bulge, -0.25);
    // Pieces are copied as they are, even invalid ones.
    const Polyline odd{{{{std::nan(""), 0.0}, 1.0}, {{0.0, 0.0}, 0.0}}, true};
    EXPECT_EQ(geom2d::pieces(odd).size(), 2U);
    EXPECT_TRUE(geom2d::pieces(Polyline{{{{0.0, 0.0}, 1.0}}, true}).empty());
}

TEST(Magnitude, IncludesMOfACurvedPieceAndAcceptsDegenerateCurves) {
    const auto curved = geom2d::magnitude(PolylineSegment{{0.0, 0.0}, {2.0, 0.0}, 4.0});
    ASSERT_TRUE(curved.hasValue());
    EXPECT_EQ(curved.value(), 4.0);
    const auto point = geom2d::magnitude(Arc{{3.0, -4.0}, 0.0, 0.0, 1.0});
    ASSERT_TRUE(point.hasValue());
    EXPECT_EQ(point.value(), 4.0);
    const auto invalid = geom2d::magnitude(Circle{{0.0, 0.0}, -1.0});
    ASSERT_FALSE(invalid.hasValue());
    EXPECT_EQ(invalid.diagnosis().code, DiagnosisCode::invalidValue);
    EXPECT_EQ(invalid.diagnosis().input, 0U);
}

TEST(Diagnosis, EndpointsAndLengthRejectDegenerateCurves) {
    const auto ends = geom2d::endpoints(Segment{{1.0, 1.0}, {1.0, 1.0}});
    ASSERT_FALSE(ends.hasValue());
    EXPECT_EQ(ends.diagnosis().code, DiagnosisCode::degenerateCurve);
    const auto size = geom2d::length(Arc{{0.0, 0.0}, 1.0, 0.0, 0x1p21});
    ASSERT_FALSE(size.hasValue());
    EXPECT_EQ(size.diagnosis().code, DiagnosisCode::outOfDomain);
}

}  // namespace
