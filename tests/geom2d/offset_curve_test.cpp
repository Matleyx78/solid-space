// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "solid_space/geom2d/offset.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <variant>

namespace {

namespace g = solid_space::geom2d;

void expectBits(double actual, double expected) {
    EXPECT_EQ(std::bit_cast<std::uint64_t>(actual), std::bit_cast<std::uint64_t>(expected));
}

void expectPointBits(g::Point2 actual, g::Point2 expected) {
    expectBits(actual.x, expected.x);
    expectBits(actual.y, expected.y);
}

void expectCurveBits(const g::Curve& actual, const g::Curve& expected) {
    ASSERT_EQ(actual.index(), expected.index());
    std::visit(
        [&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            const auto& other = std::get<T>(expected);
            if constexpr (std::is_same_v<T, g::Circle> || std::is_same_v<T, g::Arc>) {
                expectPointBits(value.center, other.center);
                expectBits(value.radius, other.radius);
                if constexpr (std::is_same_v<T, g::Arc>) {
                    expectBits(value.startAngle, other.startAngle);
                    expectBits(value.sweep, other.sweep);
                }
            } else {
                expectPointBits(value.start, other.start);
                expectPointBits(value.end, other.end);
                if constexpr (std::is_same_v<T, g::PolylineSegment>) {
                    expectBits(value.bulge, other.bulge);
                }
            }
        },
        actual);
}

TEST(OffsetCurve, BothSignedZeroDistancesPreserveEveryField) {
    const std::array<g::Curve, 5> curves{
        g::Segment{{-0.0, 1.0}, {3.0, -0.0}}, g::Circle{{-0.0, 1.0}, 2.0},
        g::Arc{{-0.0, 1.0}, 2.0, -0.0, -1.25}, g::PolylineSegment{{-0.0, 1.0}, {3.0, -0.0}, -0.0},
        g::PolylineSegment{{-0.0, 1.0}, {3.0, -0.0}, -0.125}};
    for (const auto& curve : curves) {
        for (const double distance : {0.0, -0.0}) {
            const auto result = g::offsetCurve(curve, distance);
            ASSERT_TRUE(result.hasValue());
            ASSERT_EQ(result.value().outcome, g::OffsetOutcome::done);
            expectCurveBits(result.value().curve, curve);
        }
    }
}

TEST(OffsetCurve, StraightOffsetsHaveTheRequestedDistanceAndRoundTrip) {
    for (const double scale : {0x1p-40, 1.0, 0x1p20}) {
        for (int i = 1; i <= 40; ++i) {
            const g::Segment line{{2.0 * scale, -3.0 * scale},
                                  {(2.0 + i) * scale, (1.0 + i) * scale}};
            const double dx = line.end.x - line.start.x;
            const double dy = line.end.y - line.start.y;
            const double norm = std::hypot(dx, dy);
            for (const double distance : {-0.125 * scale, 0.125 * scale}) {
                for (const bool piece : {false, true}) {
                    const g::Curve curve =
                        piece ? g::Curve(g::PolylineSegment{line.start, line.end, -0.0})
                              : g::Curve(line);
                    const auto moved = g::offsetCurve(curve, distance);
                    ASSERT_TRUE(moved.hasValue());
                    ASSERT_EQ(moved.value().outcome, g::OffsetOutcome::done);
                    const auto ends = g::endpoints(moved.value().curve);
                    ASSERT_TRUE(ends.hasValue());
                    const double error = g::kRelativePointError * g::magnitude(curve).value();
                    for (const auto p : {ends.value().start, ends.value().end}) {
                        const double signedDistance =
                            (dx * (p.y - line.start.y) - dy * (p.x - line.start.x)) / norm;
                        EXPECT_NEAR(signedDistance, distance, error);
                    }
                    const auto back = g::offsetCurve(moved.value().curve, -distance);
                    ASSERT_TRUE(back.hasValue());
                    const auto restored = g::endpoints(back.value().curve);
                    ASSERT_TRUE(restored.hasValue());
                    EXPECT_NEAR(restored.value().start.x, line.start.x, 2.0 * error);
                    EXPECT_NEAR(restored.value().start.y, line.start.y, 2.0 * error);
                    EXPECT_NEAR(restored.value().end.x, line.end.x, 2.0 * error);
                    EXPECT_NEAR(restored.value().end.y, line.end.y, 2.0 * error);
                }
            }
        }
    }
}

TEST(OffsetCurve, CirclesAndArcsRoundTripExactlyWhenRadiusDoes) {
    for (int i = 1; i <= 40; ++i) {
        for (const double sweep : {-2.0, 2.0}) {
            const std::array<g::Curve, 2> curves{
                g::Circle{{2.0, -3.0}, static_cast<double>(i)},
                g::Arc{{2.0, -3.0}, static_cast<double>(i), 0.25, sweep}};
            for (const auto& curve : curves) {
                for (const double distance : {-0.125, 0.125}) {
                    const auto moved = g::offsetCurve(curve, distance);
                    ASSERT_TRUE(moved.hasValue());
                    ASSERT_EQ(moved.value().outcome, g::OffsetOutcome::done);
                    const auto back = g::offsetCurve(moved.value().curve, -distance);
                    ASSERT_TRUE(back.hasValue());
                    expectCurveBits(back.value().curve, curve);
                    const auto ends = g::endpoints(moved.value().curve);
                    ASSERT_TRUE(ends.hasValue());
                    const double radialShift =
                        std::holds_alternative<g::Arc>(curve) && sweep < 0 ? distance : -distance;
                    const double expectedRadius = static_cast<double>(i) + radialShift;
                    for (const auto p : {ends.value().start, ends.value().end}) {
                        EXPECT_NEAR(std::hypot(p.x - 2.0, p.y + 3.0), expectedRadius,
                                    g::kRelativePointError * std::max(3.0, expectedRadius));
                    }
                }
            }
        }
    }
}

TEST(OffsetCurve, CurvedPiecesKeepBulgeAndRadialDistanceAndRoundTrip) {
    for (const double scale : {0x1p-30, 1.0, 0x1p20}) {
        for (const double bulge : {-2.0, -1.0, -0.125, 0.125, 1.0, 2.0}) {
            const g::PolylineSegment piece{{0.0, 0.0}, {8.0 * scale, 0.0}, bulge};
            const g::Point2 center{4.0 * scale, 2.0 * scale * (1.0 - bulge * bulge) / bulge};
            const double radius = std::hypot(center.x, center.y);
            const double error = g::kRelativePointError * g::magnitude(piece).value();
            for (const double distance : {-0.25 * scale, 0.25 * scale}) {
                const auto moved = g::offsetCurve(piece, distance);
                ASSERT_TRUE(moved.hasValue());
                ASSERT_EQ(moved.value().outcome, g::OffsetOutcome::done);
                const auto& result = std::get<g::PolylineSegment>(moved.value().curve);
                expectBits(result.bulge, piece.bulge);
                for (const auto p : {result.start, result.end}) {
                    EXPECT_NEAR(std::hypot(p.x - center.x, p.y - center.y),
                                radius - std::copysign(1.0, bulge) * distance, error);
                }
                const auto back = g::offsetCurve(result, -distance);
                ASSERT_TRUE(back.hasValue());
                ASSERT_EQ(back.value().outcome, g::OffsetOutcome::done);
                const auto& restored = std::get<g::PolylineSegment>(back.value().curve);
                EXPECT_NEAR(restored.start.x, piece.start.x, 2.0 * error);
                EXPECT_NEAR(restored.start.y, piece.start.y, 2.0 * error);
                EXPECT_NEAR(restored.end.x, piece.end.x, 2.0 * error);
                EXPECT_NEAR(restored.end.y, piece.end.y, 2.0 * error);
            }
        }
    }
}

TEST(OffsetCurve, IrrationalRadiusIsComparedBeforeRounding) {
    // R = sqrt(5)/2. The nearest double is above the exact radius; the
    // preceding double is below it (squaring these dyadics proves both).
    const g::PolylineSegment piece{{0.0, 0.0}, {1.0, 2.0}, 1.0};
    constexpr double kRoundedRadius = 0x1.1e3779b97f4a8p0;
    const auto vanished = g::offsetCurve(piece, kRoundedRadius);
    ASSERT_TRUE(vanished.hasValue());
    EXPECT_EQ(vanished.value().outcome, g::OffsetOutcome::vanished);
    const auto tiny = g::offsetCurve(piece, std::nextafter(kRoundedRadius, 0.0));
    ASSERT_TRUE(tiny.hasValue());
    EXPECT_EQ(tiny.value().outcome, g::OffsetOutcome::done);
}

TEST(OffsetCurve, DomainChecksUseExactCoordinatesAndOnlyTheActualArc) {
    constexpr double kLimit = solid_space::robust::kMaxCoordinate;
    const auto roundedBoundary = g::offsetCurve(g::Segment{{kLimit, 0.0}, {kLimit, 1.0}}, -0x1p-20);
    ASSERT_FALSE(roundedBoundary.hasValue());
    EXPECT_EQ(roundedBoundary.diagnosis().code, g::DiagnosisCode::outOfDomain);
    EXPECT_EQ(roundedBoundary.diagnosis().input, 1U);
    // Left arc: its support extends outside the domain on the right.
    const auto arc = g::offsetCurve(g::Arc{{kLimit, 0.0}, 1.0, 2.0, 2.0}, -1.0);
    ASSERT_TRUE(arc.hasValue());
    EXPECT_EQ(arc.value().outcome, g::OffsetOutcome::done);
    const auto circle = g::offsetCurve(g::Circle{{kLimit, 0.0}, 1.0}, -1.0);
    ASSERT_FALSE(circle.hasValue());
    EXPECT_EQ(circle.diagnosis().code, g::DiagnosisCode::outOfDomain);
}

}  // namespace
