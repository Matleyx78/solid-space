// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "curve_meaning.hpp"
#include "exact/dyadic.hpp"
#include "offset_joints.hpp"
#include "solid_space/geom2d/offset.hpp"
#include "solid_space/robust/tolerance.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {

namespace g = solid_space::geom2d;
namespace detail = solid_space::geom2d::detail;
namespace exact = solid_space::geom2d::exact;

using exact::Dyadic;

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

constexpr double kLimit = solid_space::robust::kMaxCoordinate;
constexpr double kPi = std::numbers::pi;

void expectOutOfDomainOnDistance(const g::Result<g::CurveOffset>& result) {
    ASSERT_FALSE(result.hasValue());
    EXPECT_EQ(result.diagnosis().code, g::DiagnosisCode::outOfDomain);
    EXPECT_EQ(result.diagnosis().input, 1U);
}

TEST(OffsetCurve, DomainChecksUseExactDataValuesNotTheExtent) {
    expectOutOfDomainOnDistance(g::offsetCurve(g::Segment{{kLimit, 0.0}, {kLimit, 1.0}}, -0x1p-20));
    // Left arc: its support extends outside the domain on the right.
    const auto arc = g::offsetCurve(g::Arc{{kLimit, 0.0}, 1.0, 2.0, 2.0}, -1.0);
    ASSERT_TRUE(arc.hasValue());
    EXPECT_EQ(arc.value().outcome, g::OffsetOutcome::done);
    // A circle is its centre and its radius, both within the domain: its
    // extent beyond it is not checked (#144).
    const auto circle = g::offsetCurve(g::Circle{{kLimit, 0.0}, 1.0}, -1.0);
    ASSERT_TRUE(circle.hasValue());
    ASSERT_EQ(circle.value().outcome, g::OffsetOutcome::done);
    EXPECT_EQ(std::get<g::Circle>(circle.value().curve).radius, 2.0);
    // #149: the exact radius K + 1/8 is beyond K, even though it rounds to K.
    expectOutOfDomainOnDistance(g::offsetCurve(g::Circle{{0.0, 0.0}, kLimit}, -0.125));
    // #149: the segment x = K - 1/8 moved right by 1/4.
    expectOutOfDomainOnDistance(
        g::offsetCurve(g::Segment{{kLimit - 0.125, 0.0}, {kLimit - 0.125, 16.0}}, -0.25));
    // #153: the exact start is beyond K; its rounded ends may coincide, but the
    // domain decides first.
    expectOutOfDomainOnDistance(
        g::offsetCurve(g::Segment{{kLimit, 0.0}, {kLimit - 0.125, 0x1.8p-33}}, -0x1p26));
    // An exact value within K is never returned beyond it: K - 2^-10 and K
    // itself both give K.
    for (const double distance : {-0.25, -0.25 + 0x1p-10, -0.25 + 0x1p-30}) {
        const auto moved =
            g::offsetCurve(g::Segment{{kLimit - 0.25, 0.0}, {kLimit - 0.25, 16.0}}, distance);
        ASSERT_TRUE(moved.hasValue());
        ASSERT_EQ(moved.value().outcome, g::OffsetOutcome::done);
        const auto& segment = std::get<g::Segment>(moved.value().curve);
        EXPECT_EQ(segment.start.x, kLimit);
        EXPECT_EQ(segment.end.x, kLimit);
    }
}

// The exact circle of a curved piece (curve_meaning.hpp): centre (cx, cy) / w
// and squared radius rho / w^2. The oracle of the tests below works on it
// with exact numbers, not with the code under test.
struct ExactCircle {
    Dyadic cx;
    Dyadic cy;
    Dyadic w;
    Dyadic rho;
};

ExactCircle exactCircleOf(const g::PolylineSegment& piece) {
    const auto meaning = detail::meaningOf(piece);
    const auto& circle = std::get<detail::ArcMeaning>(meaning.support).circle;
    return {circle.centerX, circle.centerY, circle.weight, circle.radiusSquared};
}

double radiusOf(const ExactCircle& circle) {
    return std::sqrt(exact::quotientToDouble(circle.rho, circle.w * circle.w));
}

g::Point2 centerOf(const ExactCircle& circle) {
    return {exact::quotientToDouble(circle.cx, circle.w),
            exact::quotientToDouble(circle.cy, circle.w)};
}

// |p - C| - R, from exact numbers: no cancellation.
double gapTo(g::Point2 p, const ExactCircle& circle) {
    const Dyadic dx = Dyadic(p.x) * circle.w - circle.cx;
    const Dyadic dy = Dyadic(p.y) * circle.w - circle.cy;
    const Dyadic power = dx * dx + dy * dy;
    const Dyadic squaredWeight = circle.w * circle.w;
    const double distance = std::sqrt(exact::quotientToDouble(power, squaredWeight));
    return exact::quotientToDouble(power - circle.rho,
                                   squaredWeight * Dyadic(distance + radiusOf(circle)));
}

// The distance of p from the ray from C through q, from exact numbers.
double acrossRay(g::Point2 p, g::Point2 q, const ExactCircle& circle) {
    const Dyadic px = Dyadic(p.x) * circle.w - circle.cx;
    const Dyadic py = Dyadic(p.y) * circle.w - circle.cy;
    const Dyadic qx = Dyadic(q.x) * circle.w - circle.cx;
    const Dyadic qy = Dyadic(q.y) * circle.w - circle.cy;
    const double qDistance =
        std::sqrt(exact::quotientToDouble(qx * qx + qy * qy, circle.w * circle.w));
    return std::fabs(
        exact::quotientToDouble(px * qy - py * qx, circle.w * circle.w * Dyadic(qDistance)));
}

double largestCoordinate(g::Point2 p) {
    return std::max(std::fabs(p.x), std::fabs(p.y));
}

// Points of the arc of a curved piece moved by `inward` towards its centre:
// near a whole turn by the centre and an angle, otherwise in the frame of the
// chord, (start + (end - start) z) with z on the standard arc, which stays
// accurate for flat arcs.
std::vector<g::Point2> pointsOf(const g::PolylineSegment& piece, double inward, int count) {
    std::vector<g::Point2> points;
    const auto circle = exactCircleOf(piece);
    const double phi = 4.0 * std::atan(piece.bulge);
    if (std::fabs(piece.bulge) > 1.0) {
        const auto center = centerOf(circle);
        const double radius = radiusOf(circle) - inward;
        const double start = std::atan2(piece.start.y - center.y, piece.start.x - center.x);
        for (int k = 0; k <= count; ++k) {
            const double angle = start + phi * k / count;
            points.push_back(
                {center.x + radius * std::cos(angle), center.y + radius * std::sin(angle)});
        }
        return points;
    }
    const double dx = piece.end.x - piece.start.x;
    const double dy = piece.end.y - piece.start.y;
    for (int k = 0; k <= count; ++k) {
        const double fraction = static_cast<double>(k) / count;
        const double scale = std::sin(phi * fraction / 2.0) / std::sin(phi / 2.0);
        const double angle = phi * (fraction - 1.0) / 2.0;
        const double zx = scale * std::cos(angle);
        const double zy = scale * std::sin(angle);
        g::Point2 q{piece.start.x + dx * zx - dy * zy, piece.start.y + dx * zy + dy * zx};
        if (inward != 0.0) {
            // Towards the centre: (C - q) / R from exact numbers.
            const double radius = radiusOf(circle);
            const double ux =
                exact::quotientToDouble(circle.cx - Dyadic(q.x) * circle.w, circle.w) / radius;
            const double uy =
                exact::quotientToDouble(circle.cy - Dyadic(q.y) * circle.w, circle.w) / radius;
            q = {q.x + inward * ux, q.y + inward * uy};
        }
        points.push_back(q);
    }
    return points;
}

// Samples of each arc in the oracle: enough to catch an arc beyond its
// error, which differs from the exact one over a large part of it.
constexpr int kOracleSamples = 64;

// The offset of a curved piece against its exact offset: each end within
// 2^-51 max(M, |p|) of its exact value, every point of either arc within
// E(A) of the circle of the other, and the same sweep (no wrap).
void expectWithinErrors(const g::PolylineSegment& input, double distance,
                        const g::PolylineSegment& result) {
    const auto circle = exactCircleOf(input);
    const double inward = input.bulge > 0.0 ? distance : -distance;
    const double magnitude = std::max(g::magnitude(input).value(), std::abs(distance));
    const auto exactPoints = pointsOf(input, inward, kOracleSamples);
    double largest = 0.0;
    for (const auto p : exactPoints) {
        largest = std::max(largest, largestCoordinate(p));
    }
    // Samples miss part of S(A): a bound below E(A) is stricter.
    const double bound = 0x1p-46 * std::max(magnitude, largest) * (1.0 - 0x1p-10);
    for (const auto& [returned, original] :
         {std::pair{result.start, input.start}, std::pair{result.end, input.end}}) {
        const double radial = gapTo(returned, circle) + inward;
        const double across = acrossRay(returned, original, circle);
        EXPECT_LE(std::sqrt(radial * radial + across * across),
                  0x1p-51 * std::max(magnitude, largestCoordinate(returned)));
    }
    for (const auto p : pointsOf(result, 0.0, kOracleSamples)) {
        EXPECT_LE(std::fabs(gapTo(p, circle) + inward), bound);
    }
    const auto returnedCircle = exactCircleOf(result);
    for (const auto p : exactPoints) {
        EXPECT_LE(std::fabs(gapTo(p, returnedCircle)), bound);
    }
    // No wrap: the included angle changes only by the rounding of the ends
    // across the chord (offset.hpp), here bounded generously.
    const double chord = std::hypot(result.end.x - result.start.x, result.end.y - result.start.y);
    const double endError = 0x1p-51 * std::max({magnitude, largestCoordinate(result.start),
                                                largestCoordinate(result.end)});
    EXPECT_NEAR(std::atan(result.bulge), std::atan(input.bulge),
                std::max(0x1p-20, 8.0 * endError / chord));
}

g::PolylineSegment arcPiece(g::Point2 center, double radius, double start, double sweep) {
    return {
        {center.x + radius * std::cos(start), center.y + radius * std::sin(start)},
        {center.x + radius * std::cos(start + sweep), center.y + radius * std::sin(start + sweep)},
        std::tan(sweep / 4.0)};
}

TEST(OffsetCurve, CurvedPiecesOfEveryAmplitudeStayWithinTheirErrors) {
    const std::array<double, 12> sweeps{0.001, 0.05, 0.3, 1.0, 2.0, 3.0,
                                        kPi,   3.5,  4.5, 5.5, 6.0, 2.0 * kPi - 0.26};
    for (const double origin : {0.0, 1e6}) {
        for (const double radius : {0.75, 3.0, 100.0}) {
            // A centre near the data, and one whose circle passes through the
            // origin, so that the arc comes back near it.
            for (const g::Point2 center :
                 {g::Point2{origin + 0.3, origin - 0.2}, g::Point2{origin + radius, origin}}) {
                for (const double start : {0.1, 2.9, 4.4}) {
                    for (const double sweep : sweeps) {
                        for (const double turn : {1.0, -1.0}) {
                            const auto input = arcPiece(center, radius, start, turn * sweep);
                            for (const double distance :
                                 {-radius / 8.0, radius / 4.0, -2.5 * radius}) {
                                const double inward = turn > 0.0 ? distance : -distance;
                                if (inward >= 0.9 * radius) {
                                    continue;
                                }
                                const auto moved = g::offsetCurve(input, distance);
                                ASSERT_TRUE(moved.hasValue());
                                ASSERT_EQ(moved.value().outcome, g::OffsetOutcome::done)
                                    << "sweep " << sweep << " radius " << radius << " origin "
                                    << origin;
                                expectWithinErrors(
                                    input, distance,
                                    std::get<g::PolylineSegment>(moved.value().curve));
                            }
                        }
                    }
                }
            }
        }
    }
}

TEST(OffsetCurve, RepresentabilityHoldsUpToTheThresholdOf177) {
    for (const double origin : {0.0, 1e6}) {
        for (const double start : {0.3, 2.0, 4.1}) {
            // Below 2 pi - 1/4 the bulge never fails.
            for (const double gap : {0.25 + 0x1p-10, 0.26, 0.3}) {
                const auto input = arcPiece({origin, origin}, 1.1, start, 2.0 * kPi - gap);
                for (const double distance : {0.125, -0.125, 0.5}) {
                    const auto moved = g::offsetCurve(input, distance);
                    ASSERT_TRUE(moved.hasValue());
                    ASSERT_EQ(moved.value().outcome, g::OffsetOutcome::done) << gap;
                    expectWithinErrors(input, distance,
                                       std::get<g::PolylineSegment>(moved.value().curve));
                }
            }
            // Above it the outcome is free (offset.hpp, bands); done still
            // keeps the errors.
            for (const double gap : {0.2, 0.05, 0.01, 1e-3, 1e-5, 1e-8}) {
                const auto input = arcPiece({origin, origin}, 1.1, start, 2.0 * kPi - gap);
                for (const double distance : {0.125, -0.125}) {
                    const auto moved = g::offsetCurve(input, distance);
                    ASSERT_TRUE(moved.hasValue());
                    if (moved.value().outcome == g::OffsetOutcome::done) {
                        expectWithinErrors(input, distance,
                                           std::get<g::PolylineSegment>(moved.value().curve));
                    } else {
                        EXPECT_EQ(moved.value().outcome, g::OffsetOutcome::failed);
                    }
                }
            }
        }
    }
}

TEST(OffsetCurve, ExactHalfCirclesKeepBulgeOneWithinItsTolerance) {
    for (const double origin : {0.0, 1e6}) {
        for (const double bulge : {1.0, -1.0}) {
            const g::PolylineSegment input{{origin, origin}, {origin + 8.0, origin}, bulge};
            for (const double distance : {1.0, -1.0, 0.5, 3.0, -7.0}) {
                if ((bulge > 0.0 ? distance : -distance) >= 4.0) {
                    continue;  // the radius 4 vanishes
                }
                const auto moved = g::offsetCurve(input, distance);
                ASSERT_TRUE(moved.hasValue());
                ASSERT_EQ(moved.value().outcome, g::OffsetOutcome::done);
                const auto& result = std::get<g::PolylineSegment>(moved.value().curve);
                const double chord =
                    std::hypot(result.end.x - result.start.x, result.end.y - result.start.y);
                const double magnitude =
                    std::max({g::magnitude(input).value(), std::abs(distance),
                              largestCoordinate(result.start), largestCoordinate(result.end)});
                // 4 E / chord, as the cases of R19 declare it.
                EXPECT_NEAR(result.bulge, bulge, 4.0 * 0x1p-46 * magnitude / chord);
                expectWithinErrors(input, distance, result);
            }
        }
    }
}

TEST(OffsetCurve, CollapsedStraightPiecesAreFreeInsideTheirBand) {
    // #148: the exact ends differ by 2^-60 per coordinate, inside the band
    // 2 kRelativePointError max(M, |p|): done with distinct ends, or failed.
    const g::Point2 a{0.0, 0.0};
    const g::Point2 b{0x1p-60, 0x1p-60};
    for (const g::Curve& curve :
         {g::Curve(g::Segment{a, b}), g::Curve(g::PolylineSegment{a, b, 0.0})}) {
        const auto moved = g::offsetCurve(curve, 1.0);
        ASSERT_TRUE(moved.hasValue());
        if (moved.value().outcome == g::OffsetOutcome::done) {
            const auto ends = g::endpoints(moved.value().curve);
            ASSERT_TRUE(ends.hasValue());
            EXPECT_FALSE(ends.value().start.x == ends.value().end.x &&
                         ends.value().start.y == ends.value().end.y);
        } else {
            EXPECT_EQ(moved.value().outcome, g::OffsetOutcome::failed);
        }
    }
}

TEST(OffsetCurve, NearlyWholeArcsOfIssues127And132) {
    // #127: a chord of one ulp at x = 1e6; with done, the radius is about
    // 0.15625 and every point within its error.
    const g::PolylineSegment first{{1e6, 0.0}, {1e6 + 0x1p-33, 0.0}, 0x1p32};
    // #132: the chord (u, 2u) at (1e6, 1e6).
    constexpr double kUlp = 0x1p-33;
    const g::PolylineSegment second{{1e6, 1e6}, {1e6 + kUlp, 1e6 + 2.0 * kUlp}, 0x1p32};
    for (const auto& input : {first, second}) {
        const auto moved = g::offsetCurve(input, -0.03125);
        ASSERT_TRUE(moved.hasValue());
        if (moved.value().outcome != g::OffsetOutcome::done) {
            EXPECT_EQ(moved.value().outcome, g::OffsetOutcome::failed);
            continue;
        }
        const auto& result = std::get<g::PolylineSegment>(moved.value().curve);
        EXPECT_NEAR(radiusOf(exactCircleOf(result)), radiusOf(exactCircleOf(input)) + 0.03125,
                    1e-6);
        expectWithinErrors(input, -0.03125, result);
    }
}

TEST(OffsetCurve, SmallAndFlatArcsKeepTheirErrorsToo) {
    // Circles of a few E at 1e6 (E is about 1.4e-8 there): the crude, sampled
    // and radial branches of representArc() (offset_joints.cpp, lemma 6).
    for (const double radius : {1e-9, 1e-8, 3e-8, 1e-7}) {
        for (const double sweep : {1.0, 3.0, 6.0}) {
            const auto input = arcPiece({1e6, 1e6}, radius, 0.0, sweep);
            const auto moved = g::offsetCurve(input, radius / 4.0);
            ASSERT_TRUE(moved.hasValue());
            ASSERT_EQ(moved.value().outcome, g::OffsetOutcome::done) << radius << " " << sweep;
            const auto& result = std::get<g::PolylineSegment>(moved.value().curve);
            if (radius >= 1e-8) {
                expectWithinErrors(input, radius / 4.0, result);
            }
        }
    }
    // Arcs far flatter than any drawing (lemma 5), and nearly flat ones.
    for (const double bulge : {1e-200, -1e-200, 1e-12, -1e-6}) {
        const g::PolylineSegment input{{0.0, 0.0}, {10.0, 0.0}, bulge};
        for (const double distance : {1.0, -1.0}) {
            const auto moved = g::offsetCurve(input, distance);
            ASSERT_TRUE(moved.hasValue());
            ASSERT_EQ(moved.value().outcome, g::OffsetOutcome::done);
            const auto& result = std::get<g::PolylineSegment>(moved.value().curve);
            // The ends move along radii tilted by at most 4 |bulge|.
            EXPECT_NEAR(result.start.y, distance, 1e-9);
            EXPECT_NEAR(result.end.y, distance, 1e-9);
            EXPECT_TRUE(result.bulge != 0.0 && std::signbit(result.bulge) == std::signbit(bulge));
        }
    }
}

TEST(OffsetCurve, HugeArcsAreScaledExactly) {
    // representArc() on an arc and on the same arc times 2^600 (beyond the
    // doubles of its computation): the same bulge and the same answer.
    for (const double sweep : {1.0, 4.0, 2.0 * kPi - 0.01}) {
        const auto input = arcPiece({3.0, -2.0}, 1.5, 0.7, sweep);
        detail::OffsetSource source;
        source.pieces.push_back(detail::meaningOf(input));
        source.distance = 0.25;
        source.magnitude = std::max(g::magnitude(input).value(), 0.25);
        const auto start =
            detail::roundedPoint(source, detail::PointRecipe::movedEnd(0, detail::PieceEnd::start));
        const auto end =
            detail::roundedPoint(source, detail::PointRecipe::movedEnd(0, detail::PieceEnd::end));
        detail::OffsetScope scope(source);
        const auto arc =
            detail::sourceArc(scope, 0, detail::PointRecipe::movedEnd(0, detail::PieceEnd::start),
                              detail::PointRecipe::movedEnd(0, detail::PieceEnd::end));
        if (!arc) {
            FAIL() << "the roots of one arc fit in a tower";
        }
        const detail::ExactArc& exactArc = *arc;
        const auto plain =
            detail::representArc(scope.tower(), exactArc, start.point, end.point, source.magnitude);
        auto huge = exactArc;
        constexpr int kGrow = 600;
        constexpr std::int64_t kGrowExact = 600;
        huge.circle.centerX = exactArc.circle.centerX.timesPowerOfTwo(kGrowExact);
        huge.circle.centerY = exactArc.circle.centerY.timesPowerOfTwo(kGrowExact);
        huge.circle.radiusSquared = exactArc.circle.radiusSquared.timesPowerOfTwo(2 * kGrowExact);
        huge.start = {exactArc.start.x.timesPowerOfTwo(kGrowExact),
                      exactArc.start.y.timesPowerOfTwo(kGrowExact), exactArc.start.weight};
        huge.end = {exactArc.end.x.timesPowerOfTwo(kGrowExact),
                    exactArc.end.y.timesPowerOfTwo(kGrowExact), exactArc.end.weight};
        const auto scaled = detail::representArc(
            scope.tower(), huge,
            {std::ldexp(start.point.x, kGrow), std::ldexp(start.point.y, kGrow)},
            {std::ldexp(end.point.x, kGrow), std::ldexp(end.point.y, kGrow)},
            std::ldexp(source.magnitude, kGrow));
        EXPECT_EQ(scaled.representable, plain.representable);
        expectBits(scaled.bulge, plain.bulge);
    }
}

// An arc in coordinates centred on a chosen point: centre, radius, start angle
// and signed sweep. The tract oracle of the tests below compares two such
// arcs point by point, not only their circles.
struct LocalArc {
    g::Point2 center;
    double radius = 0.0;
    double start = 0.0;
    double sweep = 0.0;
};

g::Point2 pointAt(const LocalArc& arc, double fraction) {
    const double angle = arc.start + arc.sweep * fraction;
    return {arc.center.x + arc.radius * std::cos(angle),
            arc.center.y + arc.radius * std::sin(angle)};
}

// The distance of p from the arc: radial within its sweep, otherwise to the
// nearer end.
double distanceToArc(g::Point2 p, const LocalArc& arc) {
    const double dx = p.x - arc.center.x;
    const double dy = p.y - arc.center.y;
    double along = std::copysign(1.0, arc.sweep) * (std::atan2(dy, dx) - arc.start);
    along = std::fmod(along, 2.0 * kPi);
    if (along < 0.0) {
        along += 2.0 * kPi;
    }
    if (along <= std::fabs(arc.sweep)) {
        return std::fabs(std::hypot(dx, dy) - arc.radius);
    }
    const auto first = pointAt(arc, 0.0);
    const auto last = pointAt(arc, 1.0);
    return std::min(std::hypot(p.x - first.x, p.y - first.y),
                    std::hypot(p.x - last.x, p.y - last.y));
}

// The two-sided distance of two arcs, from samples of each.
double tractDistance(const LocalArc& a, const LocalArc& b) {
    constexpr int kTractSamples = 512;
    double largest = 0.0;
    for (int k = 0; k <= kTractSamples; ++k) {
        const double fraction = static_cast<double>(k) / kTractSamples;
        largest = std::max({largest, distanceToArc(pointAt(a, fraction), b),
                            distanceToArc(pointAt(b, fraction), a)});
    }
    return largest;
}

// The arc of a bulge between two points, in the same coordinates.
LocalArc arcOfBulge(g::Point2 start, g::Point2 end, double bulge) {
    const double vx = end.x - start.x;
    const double vy = end.y - start.y;
    const double chord = std::hypot(vx, vy);
    const double toCenter = (1.0 - bulge * bulge) / (4.0 * bulge);
    LocalArc arc;
    arc.center = {(start.x + end.x) / 2.0 - vy * toCenter, (start.y + end.y) / 2.0 + vx * toCenter};
    arc.radius = chord * (1.0 + bulge * bulge) / (4.0 * std::fabs(bulge));
    arc.start = std::atan2(start.y - arc.center.y, start.x - arc.center.x);
    arc.sweep = 4.0 * std::atan(bulge);
    return arc;
}

// The branch of representArc() that decides the offset of one curved piece,
// from the same steps as offsetCurve() (as HugeArcsAreScaledExactly).
detail::ArcCheck branchOf(const g::PolylineSegment& input, double distance) {
    detail::OffsetSource source;
    source.pieces.push_back(detail::meaningOf(input));
    source.distance = distance;
    source.magnitude = std::max(g::magnitude(input).value(), std::abs(distance));
    const auto startRecipe = detail::PointRecipe::movedEnd(0, detail::PieceEnd::start);
    const auto endRecipe = detail::PointRecipe::movedEnd(0, detail::PieceEnd::end);
    const auto start = detail::roundedPoint(source, startRecipe);
    const auto end = detail::roundedPoint(source, endRecipe);
    detail::OffsetScope scope(source);
    const auto arc = detail::sourceArc(scope, 0, startRecipe, endRecipe);
    if (!arc) {
        return detail::ArcCheck::none;
    }
    return detail::representArc(scope.tower(), *arc, start.point, end.point, source.magnitude)
        .check;
}

TEST(OffsetCurve, ArcsLeftToTheSampledCheckAreDoneWithinTheirError) {
    // Regression of the review of #183: arcs whose radius is a fraction of
    // E(A) or a few E(A), so that neither the crude test nor the radial branch
    // of representArc() decides (R < 64 eta) and the sampled check must. Each
    // has distinct rounded ends and an exact sweep below 2 pi - 1/4: done,
    // with both ends within 2^-51 max(M, |p|) of the exact ones and every
    // point of each arc within E(A) of the other arc (not of its circle).
    int sampled = 0;
    for (const double origin : {1.0, 1e6}) {
        const double unit = 0x1p-46 * origin;  // about E(A) for these arcs
        for (const double size : {0.6, 1.0, 1.5, 2.5, 4.0}) {
            for (const double sweep : {0.5, 1.5, 3.0, 4.5, 2.0 * kPi - 0.26}) {
                for (const double turn : {1.0, -1.0}) {
                    for (const double start : {0.3, 2.2, 5.0}) {
                        const double radius = size * unit;
                        const auto input = arcPiece({origin, origin}, radius, start, turn * sweep);
                        const double distance = radius / 4.0;
                        if (branchOf(input, distance) != detail::ArcCheck::sampled) {
                            continue;
                        }
                        ++sampled;
                        const auto moved = g::offsetCurve(input, distance);
                        ASSERT_TRUE(moved.hasValue());
                        ASSERT_EQ(moved.value().outcome, g::OffsetOutcome::done)
                            << "origin " << origin << " size " << size << " sweep " << sweep
                            << " turn " << turn << " start " << start;
                        const auto& result = std::get<g::PolylineSegment>(moved.value().curve);
                        ASSERT_FALSE(result.start.x == result.end.x &&
                                     result.start.y == result.end.y);
                        // Coordinates centred on the exact centre C, from exact
                        // differences: the arcs are a few E across.
                        const auto circle = exactCircleOf(input);
                        const auto local = [&](g::Point2 p) {
                            return g::Point2{exact::quotientToDouble(
                                                 Dyadic(p.x) * circle.w - circle.cx, circle.w),
                                             exact::quotientToDouble(
                                                 Dyadic(p.y) * circle.w - circle.cy, circle.w)};
                        };
                        const double inputRadius = radiusOf(circle);
                        const double inward = input.bulge > 0.0 ? distance : -distance;
                        const double movedRadius = inputRadius - inward;
                        const auto inputStart = local(input.start);
                        const auto inputEnd = local(input.end);
                        const LocalArc exactArc{{0.0, 0.0},
                                                movedRadius,
                                                std::atan2(inputStart.y, inputStart.x),
                                                4.0 * std::atan(input.bulge)};
                        const double magnitude =
                            std::max(g::magnitude(input).value(), std::abs(distance));
                        const auto center = centerOf(circle);
                        double largest = 0.0;
                        for (int k = 0; k <= 64; ++k) {
                            const auto p = pointAt(exactArc, k / 64.0);
                            largest = std::max(largest,
                                               largestCoordinate({p.x + center.x, p.y + center.y}));
                        }
                        const double bound = 0x1p-46 * std::max(magnitude, largest);
                        const auto returnedStart = local(result.start);
                        const auto returnedEnd = local(result.end);
                        const double scale = movedRadius / inputRadius;
                        for (const auto& [returned, original, point] :
                             {std::tuple{returnedStart, inputStart, result.start},
                              std::tuple{returnedEnd, inputEnd, result.end}}) {
                            const double endBound =
                                0x1p-51 * std::max(magnitude, largestCoordinate(point));
                            EXPECT_LE(std::fabs(returned.x - original.x * scale), endBound);
                            EXPECT_LE(std::fabs(returned.y - original.y * scale), endBound);
                        }
                        EXPECT_LE(
                            tractDistance(arcOfBulge(returnedStart, returnedEnd, result.bulge),
                                          exactArc),
                            bound)
                            << "origin " << origin << " size " << size << " sweep " << sweep;
                    }
                }
            }
        }
    }
    EXPECT_GT(sampled, 50);
}

TEST(OffsetCurve, AnArcWhoseRoundedEndsCoincideFails) {
    // A chord of two ulp at (1e6, 1e6), symmetric about y = 1e6, nearly a
    // whole turn of radius R = 2^-14 + 2^-54; moved inwards to R / 16, both
    // ends round to the same double. A nonzero exact arc must not become a
    // point: failed, not a degenerate curve.
    const g::PolylineSegment input{{1e6, 1e6 - 0x1p-33}, {1e6, 1e6 + 0x1p-33}, 0x1p20};
    const double radius = 0x1p-14 + 0x1p-54;
    ASSERT_EQ(radiusOf(exactCircleOf(input)), radius);
    const auto moved = g::offsetCurve(input, radius * 15.0 / 16.0);
    ASSERT_TRUE(moved.hasValue());
    EXPECT_EQ(moved.value().outcome, g::OffsetOutcome::failed);
}

}  // namespace
