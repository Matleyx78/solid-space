// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Unit tests of tangentsFrom and tangentCircles (stage 0.2, package P15), and
// of the positions of points with nested roots (tangency_internal.hpp).
//
// The properties of ADR 0011 ("Verifica") are checked on data generated
// deterministically (a fixed SplitMix64 sequence, with its own conversion to
// doubles, so that every system draws the same data), including almost
// degenerate pairs: almost parallel lines, almost tangent and almost
// concentric circles, radii at an ulp from the critical ones. The exact
// expectations of the cases of the requests are in tests/corpus/ ([0.2-R15a],
// [0.2-R15b]); the few hand-made cases here have their answer worked out in
// the comments.

#include "solid_space/geom2d/tangency.hpp"

#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "exact/tower.hpp"
#include "offset_carriers.hpp"
#include "positions.hpp"
#include "solid_space/geom2d/side.hpp"
#include "solid_space/robust/point.hpp"
#include "tangency_internal.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <utility>
#include <variant>
#include <vector>

namespace {

using solid_space::geom2d::Arc;
using solid_space::geom2d::Circle;
using solid_space::geom2d::Curve;
using solid_space::geom2d::DiagnosisCode;
using solid_space::geom2d::Extension;
using solid_space::geom2d::kRelativePointError;
using solid_space::geom2d::Location;
using solid_space::geom2d::Placement;
using solid_space::geom2d::Point2;
using solid_space::geom2d::PolylineSegment;
using solid_space::geom2d::Segment;
using solid_space::geom2d::Side;
using solid_space::geom2d::TangencyPoint;
using solid_space::geom2d::TangentCircle;
using solid_space::geom2d::tangentCircles;
using solid_space::geom2d::tangentsFrom;
using solid_space::geom2d::detail::CurveMeaning;
using solid_space::geom2d::exact::Tower;

namespace detail = solid_space::geom2d::detail;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

double above(double value) {
    return std::nextafter(value, kInfinity);
}

double below(double value) {
    return std::nextafter(value, -kInfinity);
}

bool sameBits(double a, double b) {
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

bool sameBits(Point2 a, Point2 b) {
    return sameBits(a.x, b.x) && sameBits(a.y, b.y);
}

bool sameBits(const TangencyPoint& a, const TangencyPoint& b) {
    return sameBits(a.point, b.point) && a.position.location == b.position.location &&
           sameBits(a.position.station, b.position.station);
}

CurveMeaning meaningOf(const Curve& curve) {
    auto checked = detail::checkedMeaning(curve, 0);
    EXPECT_TRUE(checked.hasValue());
    return std::move(checked).value();
}

// A deterministic sequence: SplitMix64, and doubles built from its bits.
class Sequence {
public:
    std::uint64_t next() {
        m_state += 0x9E3779B97F4A7C15U;
        std::uint64_t z = m_state;
        z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9U;
        z = (z ^ (z >> 27U)) * 0x94D049BB133111EBU;
        return z ^ (z >> 31U);
    }

    // A double in [-limit, limit), with 53 random bits.
    double uniform(double limit) {
        const auto bits = static_cast<double>(next() >> 11U) * 0x1p-53;
        return ((2.0 * bits) - 1.0) * limit;
    }

    // A double in [low, high).
    double between(double low, double high) {
        const auto bits = static_cast<double>(next() >> 11U) * 0x1p-53;
        return low + ((high - low) * bits);
    }

    Point2 point(double limit) { return Point2{uniform(limit), uniform(limit)}; }

    std::size_t index(std::size_t count) { return static_cast<std::size_t>(next() % count); }

private:
    std::uint64_t m_state = 0x50150A2D0001;
};

// --- The carriers in floating point, for the checks --------------------------

// The circle of a circular curve, from its doubles (the centre and the radius
// of a curved PolylineSegment as curve_meaning.hpp defines them).
struct FloatCircle {
    Point2 center;
    double radius = 0.0;
};

bool isStraight(const Curve& curve) {
    if (std::holds_alternative<Segment>(curve)) {
        return true;
    }
    if (const auto* piece = std::get_if<PolylineSegment>(&curve)) {
        return piece->bulge == 0.0;
    }
    return false;
}

FloatCircle circleOf(const Curve& curve) {
    if (const auto* arc = std::get_if<Arc>(&curve)) {
        return {arc->center, arc->radius};
    }
    if (const auto* circle = std::get_if<Circle>(&curve)) {
        return {circle->center, circle->radius};
    }
    const auto& piece = std::get<PolylineSegment>(curve);
    const double dx = piece.end.x - piece.start.x;
    const double dy = piece.end.y - piece.start.y;
    const double b = piece.bulge;
    const double scale = ((b * b) - 1.0) / (4.0 * b);
    const Point2 mid{(piece.start.x + piece.end.x) / 2.0, (piece.start.y + piece.end.y) / 2.0};
    return {{mid.x + (scale * dy), mid.y - (scale * dx)},
            std::hypot(dx, dy) * (1.0 + (b * b)) / (4.0 * std::fabs(b))};
}

Point2 lineStart(const Curve& curve) {
    if (const auto* segment = std::get_if<Segment>(&curve)) {
        return segment->start;
    }
    return std::get<PolylineSegment>(curve).start;
}

Point2 lineEnd(const Curve& curve) {
    if (const auto* segment = std::get_if<Segment>(&curve)) {
        return segment->end;
    }
    return std::get<PolylineSegment>(curve).end;
}

// The signed distance of a point from the line of a straight curve, positive
// on its left.
double lineDistance(const Curve& curve, Point2 point) {
    const Point2 start = lineStart(curve);
    const Point2 end = lineEnd(curve);
    const double ux = end.x - start.x;
    const double uy = end.y - start.y;
    return ((ux * (point.y - start.y)) - (uy * (point.x - start.x))) / std::hypot(ux, uy);
}

double distanceFromCentre(const FloatCircle& circle, Point2 point) {
    return std::hypot(point.x - circle.center.x, point.y - circle.center.y);
}

double magnitudeOf(Point2 point) {
    return std::max(std::fabs(point.x), std::fabs(point.y));
}

// --- Generated data ----------------------------------------------------------

Curve randomStraight(Sequence& sequence, double limit) {
    const Point2 start = sequence.point(limit);
    Point2 end = sequence.point(limit);
    if (start.x == end.x && start.y == end.y) {
        end.x = above(end.x);
    }
    if (sequence.index(2) == 0) {
        return Segment{start, end};
    }
    return PolylineSegment{start, end, 0.0};
}

Curve randomCircular(Sequence& sequence, double limit) {
    switch (sequence.index(3)) {
        case 0:
            return Circle{sequence.point(limit), sequence.between(limit / 100.0, limit)};
        case 1:
            return Arc{sequence.point(limit), sequence.between(limit / 100.0, limit),
                       sequence.uniform(4.0), sequence.uniform(7.0)};
        default: {
            const Point2 start = sequence.point(limit);
            Point2 end = sequence.point(limit);
            if (start.x == end.x && start.y == end.y) {
                end.x = above(end.x);
            }
            double bulge = sequence.uniform(3.0);
            if (bulge == 0.0) {
                bulge = 0.5;
            }
            return PolylineSegment{start, end, bulge};
        }
    }
}

Curve randomCurve(Sequence& sequence, double limit) {
    return sequence.index(3) == 0 ? randomStraight(sequence, limit)
                                  : randomCircular(sequence, limit);
}

// A pair of curves, ordinary or almost degenerate.
struct Pair {
    Curve first;
    Curve second;
    double radius = 0.0;
};

Pair randomPair(Sequence& sequence) {
    constexpr double kLimit = 100.0;
    switch (sequence.index(6)) {
        case 0: {
            // Almost parallel lines: the second end moved by a few ulp.
            const Point2 start = sequence.point(kLimit);
            const Point2 end = sequence.point(kLimit);
            const Point2 shift = sequence.point(kLimit);
            Point2 moved{end.x + shift.x, end.y + shift.y};
            for (std::size_t i = sequence.index(4); i > 0; --i) {
                moved.y = above(moved.y);
            }
            return {Segment{start, end}, Segment{{start.x + shift.x, start.y + shift.y}, moved},
                    sequence.between(0.1, kLimit)};
        }
        case 1: {
            // Two circles and a radius at an ulp from making them tangent to a
            // circle between them: R1 + R2 + 2 r = distance.
            const Point2 centre = sequence.point(kLimit);
            const double r1 = sequence.between(1.0, 10.0);
            const double r2 = sequence.between(1.0, 10.0);
            const double radius = sequence.between(1.0, 10.0);
            const double gap = r1 + r2 + (2.0 * radius);
            double radiusNear = radius;
            switch (sequence.index(3)) {
                case 0:
                    radiusNear = above(radius);
                    break;
                case 1:
                    radiusNear = below(radius);
                    break;
                default:
                    break;
            }
            return {Circle{centre, r1}, Circle{{centre.x + gap, centre.y}, r2}, radiusNear};
        }
        case 2: {
            // Almost concentric circles.
            const Point2 centre = sequence.point(kLimit);
            const Point2 near{above(centre.x), centre.y};
            return {Arc{centre, sequence.between(1.0, 10.0), sequence.uniform(3.0),
                        sequence.uniform(6.0)},
                    Circle{near, sequence.between(1.0, 10.0)}, sequence.between(0.5, 10.0)};
        }
        case 3: {
            // A line almost tangent to a circle, at the distance 2 r.
            const Point2 centre = sequence.point(kLimit);
            const double r = sequence.between(1.0, 10.0);
            const double radius = sequence.between(0.5, 5.0);
            const double y = centre.y + r + (2.0 * radius);
            return {Segment{{centre.x - 20.0, y}, {centre.x + 20.0, above(y)}}, Circle{centre, r},
                    radius};
        }
        default:
            return {randomCurve(sequence, kLimit), randomCurve(sequence, kLimit),
                    sequence.between(0.1, kLimit)};
    }
}

// --- Checks ------------------------------------------------------------------

// The magnitude M of a call, as tangency.hpp defines it.
double magnitudeOf(const Pair& pair) {
    return std::max({solid_space::geom2d::magnitude(pair.first).value(),
                     solid_space::geom2d::magnitude(pair.second).value(), pair.radius});
}

// The station of a point of the carrier, computed apart in floating point
// (std::atan2 is fine in a test): the signed distance along a line, the radius
// times the angle from the start in the direction of the curve.
double expectedStation(const Curve& curve, Point2 point) {
    if (isStraight(curve)) {
        const Point2 start = lineStart(curve);
        const Point2 end = lineEnd(curve);
        const double ux = end.x - start.x;
        const double uy = end.y - start.y;
        return ((ux * (point.x - start.x)) + (uy * (point.y - start.y))) / std::hypot(ux, uy);
    }
    const auto carrier = circleOf(curve);
    double startAngle = 0.0;
    double turn = 1.0;
    if (const auto* arc = std::get_if<Arc>(&curve)) {
        startAngle = arc->startAngle;
        turn = arc->sweep > 0.0 ? 1.0 : -1.0;
    } else if (const auto* piece = std::get_if<PolylineSegment>(&curve)) {
        startAngle =
            std::atan2(piece->start.y - carrier.center.y, piece->start.x - carrier.center.x);
        turn = piece->bulge > 0.0 ? 1.0 : -1.0;
    }
    const double angle =
        std::atan2(point.y - carrier.center.y, point.x - carrier.center.x) - startAngle;
    const double fullTurn = 2.0 * std::numbers::pi;
    return carrier.radius * std::fmod(std::fmod(turn * angle, fullTurn) + fullTurn, fullTurn);
}

// The station agrees with expectedStation(), over the whole circle (a point
// at the start may be read at 0 or at a whole turn).
void checkStation(const Curve& curve, const TangencyPoint& point, double magnitude) {
    const double expected = expectedStation(curve, point.point);
    double difference = std::fabs(point.position.station - expected);
    if (!isStraight(curve)) {
        const double whole = 2.0 * std::numbers::pi * circleOf(curve).radius;
        difference = std::min(difference, std::fabs(difference - whole));
    }
    const double scale = std::max({magnitude, std::fabs(expected), magnitudeOf(point.point)});
    EXPECT_LE(difference, 64.0 * kRelativePointError * scale);
}

// The point of tangency lies on the carrier of the curve, at distance r from
// the centre, and the centre at distance r from the curve, all within the
// errors declared for points (the evaluation in double adds a few ulp of the
// magnitude, well inside the factor 16 of the bound used here).
void checkTangency(const Curve& curve, const TangentCircle& circle, const TangencyPoint& point,
                   double radius, double magnitude) {
    const double scale =
        std::max({magnitude, magnitudeOf(circle.center), magnitudeOf(point.point)});
    const double tolerance = 16.0 * kRelativePointError * scale;
    EXPECT_NEAR(std::hypot(point.point.x - circle.center.x, point.point.y - circle.center.y),
                radius, tolerance);
    checkStation(curve, point, magnitude);
    if (isStraight(curve)) {
        EXPECT_NEAR(lineDistance(curve, point.point), 0.0, tolerance);
        EXPECT_NEAR(std::fabs(lineDistance(curve, circle.center)), radius, tolerance);
        return;
    }
    const auto carrier = circleOf(curve);
    EXPECT_NEAR(distanceFromCentre(carrier, point.point), carrier.radius, tolerance);
    const double fromCentre = distanceFromCentre(carrier, circle.center);
    const double outside = std::fabs(fromCentre - (carrier.radius + radius));
    const double inside = std::fabs(fromCentre - std::fabs(carrier.radius - radius));
    EXPECT_LE(std::min(outside, inside), tolerance);
}

// --- tangentsFrom ------------------------------------------------------------

TEST(TangentsFrom, PointsAreTangentAndInOrder) {
    Sequence sequence;
    std::size_t outsideCount = 0;
    for (int i = 0; i < 400; ++i) {
        const Curve curve = randomCircular(sequence, 100.0);
        const Point2 point = sequence.point(300.0);
        const auto result = tangentsFrom(point, curve, Extension::extended);
        ASSERT_TRUE(result.hasValue());
        const auto& tangents = result.value();
        const auto carrier = circleOf(curve);
        const double magnitude =
            std::max(solid_space::geom2d::magnitude(curve).value(), magnitudeOf(point));
        if (tangents.placement != Placement::outside) {
            continue;
        }
        ++outsideCount;
        ASSERT_EQ(tangents.points.size(), 2U);
        for (const auto& tangent : tangents.points) {
            const double tolerance =
                16.0 * kRelativePointError * std::max(magnitude, magnitudeOf(tangent.point));
            EXPECT_NEAR(distanceFromCentre(carrier, tangent.point), carrier.radius, tolerance);
            checkStation(curve, tangent, magnitude);
            // (T - C) . (T - Q) = 0: the line QT is tangent at T.
            const double tx = tangent.point.x - carrier.center.x;
            const double ty = tangent.point.y - carrier.center.y;
            const double dot =
                (tx * (tangent.point.x - point.x)) + (ty * (tangent.point.y - point.y));
            EXPECT_NEAR(dot, 0.0, tolerance * 8.0 * magnitude);
        }
        EXPECT_LE(tangents.points[0].position.station, tangents.points[1].position.station);

        // With Extension::none, exactly the points on the curve as given.
        const auto given = tangentsFrom(point, curve, Extension::none);
        ASSERT_TRUE(given.hasValue());
        std::size_t onCurve = 0;
        for (const auto& tangent : tangents.points) {
            const auto location = tangent.position.location;
            if (location == Location::atStart || location == Location::inside ||
                location == Location::atEnd) {
                ASSERT_LT(onCurve, given.value().points.size());
                EXPECT_TRUE(sameBits(tangent, given.value().points[onCurve]));
                ++onCurve;
            }
        }
        EXPECT_EQ(onCurve, given.value().points.size());
    }
    EXPECT_GT(outsideCount, 100U);
}

// Points exactly on circles, and their neighbours: tangentsFrom gives on
// exactly when side() gives on (ADR 0011, "Verifica").
TEST(TangentsFrom, OnExactlyWhenSideIsOn) {
    Sequence sequence;
    std::size_t onCount = 0;
    for (int i = 0; i < 300; ++i) {
        std::vector<std::pair<Curve, Point2>> cases;
        // A centre of doubles and a radius 5 s with s a power of two: the point
        // centre + (3 s, 4 s) is on the circle when the sums are exact, which
        // holds for small integers.
        const Point2 centre{std::round(sequence.uniform(1000.0)),
                            std::round(sequence.uniform(1000.0))};
        const double s = std::ldexp(1.0, static_cast<int>(sequence.index(20)) - 10);
        const Point2 on{centre.x + (3.0 * s), centre.y + (4.0 * s)};
        cases.emplace_back(Circle{centre, 5.0 * s}, on);
        cases.emplace_back(Arc{centre, 5.0 * s, sequence.uniform(3.0), sequence.uniform(6.0)}, on);
        // The ends of a curved polyline piece lie on its circle exactly.
        const Point2 start = sequence.point(100.0);
        const Point2 end = sequence.point(100.0);
        const PolylineSegment piece{start, end, sequence.uniform(2.0) + 0.25};
        cases.emplace_back(piece, start);
        cases.emplace_back(piece, end);
        for (const auto& [curve, point] : cases) {
            for (const Point2 probe :
                 {point, Point2{above(point.x), point.y}, Point2{below(point.x), point.y},
                  Point2{point.x, above(point.y)}}) {
                const auto placed = tangentsFrom(probe, curve, Extension::extended);
                const auto sideOf = solid_space::geom2d::side(curve, probe);
                ASSERT_TRUE(placed.hasValue());
                ASSERT_TRUE(sideOf.hasValue());
                EXPECT_EQ(placed.value().placement == Placement::on, sideOf.value() == Side::on);
                if (placed.value().placement == Placement::on) {
                    ++onCount;
                    ASSERT_EQ(placed.value().points.size(), 1U);
                    EXPECT_TRUE(sameBits(placed.value().points[0].point, probe));
                }
            }
        }
    }
    EXPECT_GT(onCount, 600U);
}

// Q at R + 1e-12 from the centre (request #21, case 6) is outside; translated
// by 1e6 it rounds onto the circle (case 7).
TEST(TangentsFrom, RequestCasesSixAndSeven) {
    const auto near = tangentsFrom({5.000000000001, 0.0}, Circle{{0.0, 0.0}, 5.0}, Extension::none);
    ASSERT_TRUE(near.hasValue());
    EXPECT_EQ(near.value().placement, Placement::outside);
    ASSERT_EQ(near.value().points.size(), 2U);
    EXPECT_GT(near.value().points[0].point.y, 0.0);
    EXPECT_LT(near.value().points[1].point.y, 0.0);

    const auto far =
        tangentsFrom({1e6 + 5.000000000001, 1e6}, Circle{{1e6, 1e6}, 5.0}, Extension::none);
    ASSERT_TRUE(far.hasValue());
    EXPECT_EQ(far.value().placement, Placement::on);
}

TEST(TangentsFrom, Diagnoses) {
    const Circle circle{{0.0, 0.0}, 1.0};
    const auto straight =
        tangentsFrom({2.0, 0.0}, Segment{{0.0, 0.0}, {1.0, 0.0}}, Extension::none);
    ASSERT_FALSE(straight.hasValue());
    EXPECT_EQ(straight.diagnosis().code, DiagnosisCode::notCircular);
    EXPECT_EQ(straight.diagnosis().input, 0U);

    // The curve is checked before the point.
    const auto both = tangentsFrom({kInfinity, 0.0}, Circle{{0.0, 0.0}, -1.0}, Extension::none);
    ASSERT_FALSE(both.hasValue());
    EXPECT_EQ(both.diagnosis().code, DiagnosisCode::invalidValue);
    EXPECT_EQ(both.diagnosis().input, 0U);

    const auto far = tangentsFrom({0x1p51, 0.0}, circle, Extension::none);
    ASSERT_FALSE(far.hasValue());
    EXPECT_EQ(far.diagnosis().code, DiagnosisCode::outOfDomain);
    EXPECT_EQ(far.diagnosis().input, 1U);
}

// --- tangentCircles ----------------------------------------------------------

TEST(TangentCircles, CirclesAreAtTheRadiusFromBothCurves) {
    Sequence sequence;
    std::size_t circleCount = 0;
    for (int i = 0; i < 600; ++i) {
        const Pair pair = randomPair(sequence);
        const auto result = tangentCircles(pair.first, pair.second, pair.radius);
        if (!result.hasValue()) {
            // Only two almost parallel lines may put a centre beyond the domain.
            EXPECT_EQ(result.diagnosis().code, DiagnosisCode::outOfDomain);
            EXPECT_EQ(result.diagnosis().input, 2U);
            continue;
        }
        const auto& circles = result.value();
        EXPECT_LE(circles.circles.size(), 8U);
        if (circles.infinitelyMany) {
            EXPECT_TRUE(circles.circles.empty());
        }
        const double magnitude = magnitudeOf(pair);
        for (const auto& circle : circles.circles) {
            ++circleCount;
            checkTangency(pair.first, circle, circle.onFirst, pair.radius, magnitude);
            checkTangency(pair.second, circle, circle.onSecond, pair.radius, magnitude);
            EXPECT_NE(circle.sideOfFirst, Side::on);
            EXPECT_NE(circle.sideOfSecond, Side::on);
        }
        // The exact order along the first curve: stations never decrease.
        for (std::size_t j = 1; j < circles.circles.size(); ++j) {
            EXPECT_LE(circles.circles[j - 1].onFirst.position.station,
                      circles.circles[j].onFirst.position.station);
        }
    }
    EXPECT_GT(circleCount, 500U);
}

TEST(TangentCircles, SwappingTheCurvesSwapsTheSides) {
    Sequence sequence;
    for (int i = 0; i < 400; ++i) {
        const Pair pair = randomPair(sequence);
        const auto direct = tangentCircles(pair.first, pair.second, pair.radius);
        const auto swapped = tangentCircles(pair.second, pair.first, pair.radius);
        ASSERT_EQ(direct.hasValue(), swapped.hasValue());
        if (!direct.hasValue()) {
            EXPECT_EQ(direct.diagnosis().code, swapped.diagnosis().code);
            continue;
        }
        const auto& a = direct.value();
        const auto& b = swapped.value();
        EXPECT_EQ(a.infinitelyMany, b.infinitelyMany);
        ASSERT_EQ(a.circles.size(), b.circles.size());
        for (const auto& circle : a.circles) {
            const auto match =
                std::count_if(b.circles.begin(), b.circles.end(), [&](const TangentCircle& other) {
                    return sameBits(circle.center, other.center) &&
                           sameBits(circle.onFirst, other.onSecond) &&
                           sameBits(circle.onSecond, other.onFirst) &&
                           circle.sideOfFirst == other.sideOfSecond &&
                           circle.sideOfSecond == other.sideOfFirst;
                });
            EXPECT_EQ(match, 1);
        }
    }
}

// The side of a tangent circle with respect to a straight curve is side() of
// its centre (the radii here are far above the error of the centre).
TEST(TangentCircles, SideOnAStraightCurveIsTheSideOfTheCentre) {
    Sequence sequence;
    std::size_t checked = 0;
    for (int i = 0; i < 400; ++i) {
        const Curve line = randomStraight(sequence, 100.0);
        const Curve other = randomCurve(sequence, 100.0);
        const auto result = tangentCircles(line, other, sequence.between(0.5, 50.0));
        if (!result.hasValue()) {
            continue;
        }
        for (const auto& circle : result.value().circles) {
            EXPECT_EQ(solid_space::geom2d::side(line, circle.center).value(), circle.sideOfFirst);
            if (isStraight(other)) {
                EXPECT_EQ(solid_space::geom2d::side(other, circle.center).value(),
                          circle.sideOfSecond);
            }
            ++checked;
        }
    }
    EXPECT_GT(checked, 300U);
}

// Two crossing circles of radius 2 with centres 3 apart, r = 0.4: the parallels
// have radii 2.4 and 1.6, and every pair of them crosses (0.8 < 3 < 4.8), so
// there are eight circles. Inside both, both sides are left (counterclockwise).
TEST(TangentCircles, EightCircles) {
    const auto result = tangentCircles(Circle{{0.0, 0.0}, 2.0}, Circle{{3.0, 0.0}, 2.0}, 0.4);
    ASSERT_TRUE(result.hasValue());
    ASSERT_EQ(result.value().circles.size(), 8U);
    std::size_t insideBoth = 0;
    for (const auto& circle : result.value().circles) {
        if (circle.sideOfFirst == Side::left && circle.sideOfSecond == Side::left) {
            ++insideBoth;
        }
    }
    EXPECT_EQ(insideBoth, 2U);
}

TEST(TangentCircles, InfinitelyMany) {
    // The same line, run in opposite directions.
    const auto line =
        tangentCircles(Segment{{0.0, 0.0}, {1.0, 1.0}}, Segment{{5.0, 5.0}, {3.0, 3.0}}, 0.25);
    ASSERT_TRUE(line.hasValue());
    EXPECT_TRUE(line.value().infinitelyMany);
    EXPECT_TRUE(line.value().circles.empty());
    // The same circle: a clockwise piece of it (bulge -1: the upper half, from
    // (-1, 0) through (0, 1) to (1, 0)) and the whole circle.
    const auto circle = tangentCircles(PolylineSegment{{-1.0, 0.0}, {1.0, 0.0}, -1.0},
                                       Circle{{0.0, 0.0}, 1.0}, 3.0);
    ASSERT_TRUE(circle.hasValue());
    EXPECT_TRUE(circle.value().infinitelyMany);
    // Concentric, radii 1 and 4: they differ by 3 = 2 r for r = 1.5, and add
    // up to 5 = 2 r for r = 2.5; not for r = 2.
    for (const double radius : {1.5, 2.5}) {
        const auto concentric =
            tangentCircles(Circle{{0.5, 0.5}, 1.0}, Circle{{0.5, 0.5}, 4.0}, radius);
        ASSERT_TRUE(concentric.hasValue());
        EXPECT_TRUE(concentric.value().infinitelyMany);
    }
    const auto none = tangentCircles(Circle{{0.5, 0.5}, 1.0}, Circle{{0.5, 0.5}, 4.0}, 2.0);
    ASSERT_TRUE(none.hasValue());
    EXPECT_FALSE(none.value().infinitelyMany);
    EXPECT_TRUE(none.value().circles.empty());
}

// Circle of radius 1 at the origin and the line y = 1, r = 1. The parallels
// of the line are y = 2 and y = 0; those of the circle are the circle of
// radius 2 and, for |R - r| = 0, the centre alone, which would only give the
// circle itself: left out. y = 2 touches the circle of radius 2 at (0, 2); y = 0
// crosses it at (-2, 0) and (2, 0). Three circles, along the line by x.
TEST(TangentCircles, TheCurveItselfIsNotATangentCircle) {
    const auto result =
        tangentCircles(Segment{{-10.0, 1.0}, {10.0, 1.0}}, Circle{{0.0, 0.0}, 1.0}, 1.0);
    ASSERT_TRUE(result.hasValue());
    const auto& circles = result.value().circles;
    ASSERT_EQ(circles.size(), 3U);
    EXPECT_TRUE(sameBits(circles[0].center, Point2{-2.0, 0.0}));
    EXPECT_TRUE(sameBits(circles[0].onSecond.point, Point2{-1.0, 0.0}));
    EXPECT_TRUE(sameBits(circles[1].center, Point2{0.0, 2.0}));
    EXPECT_TRUE(sameBits(circles[1].onFirst.point, Point2{0.0, 1.0}));
    EXPECT_EQ(circles[1].sideOfFirst, Side::left);
    EXPECT_TRUE(sameBits(circles[2].center, Point2{2.0, 0.0}));
    EXPECT_EQ(circles[2].sideOfFirst, Side::right);
    EXPECT_EQ(circles[2].sideOfSecond, Side::right);
}

// A circle of radius 3 inside a counterclockwise circle of radius 1 encloses
// it: its side is right, as for a circle outside (tangency.hpp). With the line
// y = 5 and r = 3: the parallel circles have radii 4 and 2, the parallels of
// the line are y = 8 and y = 2. y = 2 touches the circle of radius 2 at
// (0, 2): that circle encloses the unit circle and touches it at (0, -1).
TEST(TangentCircles, EnclosingCircleIsOnTheRight) {
    const auto result =
        tangentCircles(Circle{{0.0, 0.0}, 1.0}, Segment{{-10.0, 5.0}, {10.0, 5.0}}, 3.0);
    ASSERT_TRUE(result.hasValue());
    const auto& circles = result.value().circles;
    const auto enclosing = std::find_if(circles.begin(), circles.end(), [](const TangentCircle& c) {
        return sameBits(c.center, Point2{0.0, 2.0});
    });
    ASSERT_NE(enclosing, circles.end());
    EXPECT_EQ(enclosing->sideOfFirst, Side::right);
    EXPECT_TRUE(sameBits(enclosing->onFirst.point, Point2{0.0, -1.0}));
    EXPECT_TRUE(sameBits(enclosing->onSecond.point, Point2{0.0, 5.0}));
    EXPECT_EQ(enclosing->sideOfSecond, Side::right);
}

// The line y = 0 and the line from (0, 1) to (1, 1 + 2^-52) cross near
// x = -2^52, beyond robust::kMaxCoordinate, and so do all four centres.
TEST(TangentCircles, CentreBeyondTheDomain) {
    const auto result = tangentCircles(Segment{{0.0, 0.0}, {1.0, 0.0}},
                                       Segment{{0.0, 1.0}, {1.0, 1.0 + 0x1p-52}}, 0.25);
    ASSERT_FALSE(result.hasValue());
    EXPECT_EQ(result.diagnosis().code, DiagnosisCode::outOfDomain);
    EXPECT_EQ(result.diagnosis().input, 2U);
}

TEST(TangentCircles, Diagnoses) {
    const Segment line{{0.0, 0.0}, {1.0, 0.0}};
    const auto bad = tangentCircles(line, Segment{{0.0, 0.0}, {0.0, 0.0}}, std::nan(""));
    ASSERT_FALSE(bad.hasValue());
    EXPECT_EQ(bad.diagnosis().code, DiagnosisCode::degenerateCurve);
    EXPECT_EQ(bad.diagnosis().input, 1U);
    const auto negativeZero = tangentCircles(line, Circle{{0.0, 0.0}, 1.0}, -0.0);
    ASSERT_FALSE(negativeZero.hasValue());
    EXPECT_EQ(negativeZero.diagnosis().code, DiagnosisCode::degenerateCurve);
    EXPECT_EQ(negativeZero.diagnosis().input, 2U);
    const auto infinite = tangentCircles(line, Circle{{0.0, 0.0}, 1.0}, kInfinity);
    ASSERT_FALSE(infinite.hasValue());
    EXPECT_EQ(infinite.diagnosis().code, DiagnosisCode::invalidValue);
    EXPECT_EQ(infinite.diagnosis().input, 2U);
}

// --- Positions of points of the tower ----------------------------------------

// The exact points of meet() (one root) taken into a tower get the same
// location, the same station bits, the same order and the same rounded point as
// positions.hpp gives them: the two headers apply one rule.
TEST(TowerPositions, AgreeWithPositionsOfExactPoints) {
    Sequence sequence;
    std::size_t compared = 0;
    for (int i = 0; i < 400; ++i) {
        const Curve a = randomCurve(sequence, 100.0);
        const Curve b = randomCurve(sequence, 100.0);
        const auto meaningA = meaningOf(a);
        const auto meaningB = meaningOf(b);
        const auto meeting = detail::meet(detail::carrierOf(meaningA), detail::carrierOf(meaningB));
        Tower tower;
        std::vector<detail::ExactPoint> exact;
        std::vector<detail::TowerPoint> lifted;
        for (const auto& found : meeting.found()) {
            if (detail::beyondDomain(found.point)) {
                continue;
            }
            const auto point = detail::towerPoint(tower, found.point);
            ASSERT_TRUE(point.has_value());
            if (!point) {
                return;
            }
            exact.push_back(found.point);
            lifted.push_back(*point);
        }
        for (std::size_t j = 0; j < exact.size(); ++j) {
            for (const auto* meaning : {&meaningA, &meaningB}) {
                const auto expected = detail::positionOn(*meaning, exact[j]);
                const auto actual = detail::positionOn(tower, *meaning, lifted[j]);
                EXPECT_EQ(actual.location, expected.location);
                EXPECT_TRUE(sameBits(actual.station, expected.station));
                for (std::size_t k = 0; k < exact.size(); ++k) {
                    EXPECT_EQ(detail::compareAlong(tower, *meaning, lifted[j], lifted[k]),
                              detail::compareAlong(*meaning, exact[j], exact[k]));
                }
            }
            EXPECT_TRUE(
                sameBits(detail::nearestPoint(tower, lifted[j]), detail::nearestPoint(exact[j])));
            ++compared;
        }
    }
    EXPECT_GT(compared, 200U);
}

// The ends of arcs, in direction D0 and D1, are atStart and atEnd, with the
// stations of positions.hpp, also through the tower.
TEST(TowerPositions, EndsOfArcs) {
    Sequence sequence;
    for (int i = 0; i < 100; ++i) {
        const Curve curve = randomCircular(sequence, 50.0);
        const auto meaning = meaningOf(curve);
        Tower tower;
        const auto start = detail::towerPoint(tower, detail::startPoint(meaning));
        const auto end = detail::towerPoint(tower, detail::endPoint(meaning));
        ASSERT_TRUE(start.has_value());
        ASSERT_TRUE(end.has_value());
        if (!start || !end) {
            return;
        }
        EXPECT_EQ(detail::locate(tower, meaning, *start),
                  detail::locate(meaning, detail::startPoint(meaning)));
        EXPECT_EQ(detail::locate(tower, meaning, *end),
                  detail::locate(meaning, detail::endPoint(meaning)));
        EXPECT_TRUE(sameBits(detail::station(tower, meaning, *end),
                             detail::station(meaning, detail::endPoint(meaning))));
    }
}

}  // namespace
