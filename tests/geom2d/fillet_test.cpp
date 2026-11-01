// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Unit tests of tangentCircleNear and fillet (stage 0.2, package P16).
//
// The properties of the package sheet are checked on data generated
// deterministically (a fixed SplitMix64 sequence, with its own conversion to
// doubles, so that every system draws the same data), at two scales: the
// fillet is a circle of tangentCircles with the sides of rule 3, the nearest
// to the picks; the ends of its arc coincide with the points of tangency, also
// for the rule of ADR 0009; the Arc and the bulge describe the same arc;
// swapping the curves and the picks gives the same circle with the arc
// reversed. The exact expectations of the cases of the request and of
// AutoCAD are in tests/corpus/ ([0.2-R16a], [0.2-R16b], [0.2-R16c]); the few
// hand-made cases here have their answer worked out in the comments.

#include "solid_space/geom2d/curves.hpp"
#include "solid_space/geom2d/intersection.hpp"
#include "solid_space/geom2d/side.hpp"
#include "solid_space/geom2d/tangency.hpp"
#include "solid_space/robust/point.hpp"
#include "solid_space/robust/tolerance.hpp"
#include "solid_space/robust/trigonometry.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <vector>

namespace {

using solid_space::geom2d::Arc;
using solid_space::geom2d::ChosenCircle;
using solid_space::geom2d::Circle;
using solid_space::geom2d::Curve;
using solid_space::geom2d::DiagnosisCode;
using solid_space::geom2d::Extension;
using solid_space::geom2d::Fillet;
using solid_space::geom2d::fillet;
using solid_space::geom2d::kRelativeArcError;
using solid_space::geom2d::kRelativePointError;
using solid_space::geom2d::Location;
using solid_space::geom2d::Point2;
using solid_space::geom2d::PolylineSegment;
using solid_space::geom2d::Segment;
using solid_space::geom2d::Side;
using solid_space::geom2d::TangencyOutcome;
using solid_space::geom2d::TangencyPoint;
using solid_space::geom2d::TangentCircle;
using solid_space::geom2d::tangentCircleNear;
using solid_space::geom2d::tangentCircles;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

double above(double value) {
    return std::nextafter(value, kInfinity);
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

bool sameCircle(const TangentCircle& a, const TangentCircle& b) {
    return sameBits(a.center, b.center) && sameBits(a.onFirst, b.onFirst) &&
           sameBits(a.onSecond, b.onSecond) && a.sideOfFirst == b.sideOfFirst &&
           a.sideOfSecond == b.sideOfSecond;
}

double magnitudeOf(Point2 point) {
    return std::max(std::fabs(point.x), std::fabs(point.y));
}

double distance(Point2 a, Point2 b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

// The nearness of tangency.hpp, written again here.
double nearness(const TangentCircle& circle, Point2 nearFirst, Point2 nearSecond) {
    const double dx1 = circle.onFirst.point.x - nearFirst.x;
    const double dy1 = circle.onFirst.point.y - nearFirst.y;
    const double dx2 = circle.onSecond.point.x - nearSecond.x;
    const double dy2 = circle.onSecond.point.y - nearSecond.y;
    return ((dx1 * dx1) + (dy1 * dy1)) + ((dx2 * dx2) + (dy2 * dy2));
}

// The index of the nearest circle, the earlier one on a tie, and whether
// another circle ties with it.
struct Nearest {
    std::size_t index = 0;
    bool tie = false;
};

Nearest nearestOf(const std::vector<TangentCircle>& circles, Point2 nearFirst, Point2 nearSecond) {
    Nearest result;
    double best = kInfinity;
    for (std::size_t i = 0; i < circles.size(); ++i) {
        const double value = nearness(circles[i], nearFirst, nearSecond);
        if (value < best) {
            best = value;
            result = {i, false};
        } else if (value == best) {
            result.tie = true;
        }
    }
    return result;
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

    std::size_t index(std::size_t count) { return static_cast<std::size_t>(next() % count); }

private:
    std::uint64_t m_state = 0x50160F111E7;
};

// A call of fillet or tangentCircleNear, around an origin (0 or far away).
struct Call {
    Curve first;
    Point2 pickFirst;
    Curve second;
    Point2 pickSecond;
    double radius = 0.0;
};

class Generator {
public:
    explicit Generator(Point2 origin) : m_origin(origin) {}

    Point2 point(double limit) {
        return Point2{m_origin.x + m_sequence.uniform(limit),
                      m_origin.y + m_sequence.uniform(limit)};
    }

    Curve curve(double limit) {
        const Point2 start = point(limit);
        Point2 end = point(limit);
        if (start.x == end.x && start.y == end.y) {
            end.x = above(end.x);
        }
        switch (m_sequence.index(5)) {
            case 0:
                return Segment{start, end};
            case 1:
                return PolylineSegment{start, end, 0.0};
            case 2:
                return Circle{start, m_sequence.between(limit / 20.0, limit)};
            case 3:
                return Arc{start, m_sequence.between(limit / 20.0, limit), m_sequence.uniform(4.0),
                           m_sequence.uniform(7.0)};
            default:
                break;
        }
        double bulge = m_sequence.uniform(2.0);
        if (bulge == 0.0) {
            bulge = 0.5;
        }
        return PolylineSegment{start, end, bulge};
    }

    Call call(double limit) {
        switch (m_sequence.index(4)) {
            case 0:
                return almostTangent(limit);
            case 1:
                return almostParallel(limit);
            default:
                break;
        }
        return Call{curve(limit), point(limit), curve(limit), point(limit),
                    m_sequence.between(limit / 100.0, limit / 2.0)};
    }

    // A line above a circle at about 2 r from it, with the picks between them:
    // the fillet exists exactly when the gap is at most 2 r, with points of
    // tangency almost at the ends of a diameter.
    Call almostTangent(double limit) {
        const Point2 centre = point(limit);
        const double circleRadius = m_sequence.between(limit / 20.0, limit / 2.0);
        const double radius = m_sequence.between(limit / 100.0, limit / 10.0);
        double y = centre.y + circleRadius + (2.0 * radius);
        for (std::size_t i = m_sequence.index(5); i > 0; --i) {
            y = std::nextafter(y, i % 2 == 0 ? kInfinity : -kInfinity);
        }
        const double shift = m_sequence.uniform(radius);
        return Call{Segment{{centre.x - limit, y}, {centre.x + limit, y}},
                    Point2{centre.x + shift, y}, Circle{centre, circleRadius},
                    Point2{centre.x + shift, centre.y + circleRadius}, radius};
    }

    // Two segments that meet at a small angle, with the picks on them.
    Call almostParallel(double limit) {
        const Point2 corner = point(limit);
        const double slope = std::ldexp(1.0, -static_cast<int>(m_sequence.index(40)));
        const Point2 before{corner.x - limit, corner.y};
        const Point2 after{corner.x + limit, corner.y + (slope * limit)};
        return Call{Segment{before, corner}, Point2{corner.x - (limit / 2.0), corner.y},
                    Segment{corner, after},
                    Point2{corner.x + (limit / 2.0), corner.y + (slope * limit / 2.0)},
                    m_sequence.between(limit / 1000.0, limit / 10.0)};
    }

    Sequence& sequence() { return m_sequence; }

private:
    Sequence m_sequence;
    Point2 m_origin;
};

// The magnitude M of a call (tangency.hpp).
double magnitudeOf(const Call& call) {
    return std::max({solid_space::geom2d::magnitude(call.first).value(),
                     solid_space::geom2d::magnitude(call.second).value(),
                     magnitudeOf(call.pickFirst), magnitudeOf(call.pickSecond), call.radius});
}

// The largest of M and the coordinates of the fillet: max(M, |p|).
double scaleOf(const Call& call, const Fillet& result) {
    return std::max({magnitudeOf(call), magnitudeOf(result.center),
                     magnitudeOf(result.onFirst.point), magnitudeOf(result.onSecond.point)});
}

Point2 pointOfArc(const Arc& arc, double angle) {
    const auto direction = solid_space::robust::sinCos(angle);
    return Point2{arc.center.x + (arc.radius * direction.cosine),
                  arc.center.y + (arc.radius * direction.sine)};
}

// The ends of the Arc form, as curves.hpp defines them (D0 and D1 from
// robust::sinCos, the sum rounded), coincide with the points of tangency
// within the declared errors (the arc against the exact points, and the
// rounding of the points), and for the tolerance rule of ADR 0009; the middle
// of the bulge form is within twice the declared error of the middle of the
// Arc form.
void checkArc(const Call& call, const Fillet& result) {
    const double scale = scaleOf(call, result);
    const double ends = (kRelativeArcError + (2.0 * kRelativePointError)) * scale;
    const Arc& arc = result.arc;
    const Point2 start = pointOfArc(arc, arc.startAngle);
    const Point2 end = pointOfArc(arc, arc.startAngle + arc.sweep);
    EXPECT_LE(distance(start, result.onFirst.point), ends);
    EXPECT_LE(distance(end, result.onSecond.point), ends);
    const auto policy = solid_space::robust::TolerancePolicy::forUnit(1.0).value();
    EXPECT_TRUE(policy.coincident(start, result.onFirst.point));
    EXPECT_TRUE(policy.coincident(end, result.onSecond.point));

    EXPECT_LE(std::fabs(arc.sweep), std::numbers::pi);
    const Point2 from = result.onFirst.point;
    const Point2 to = result.onSecond.point;
    const double half = result.bulge / 2.0;
    const Point2 middleOfBulge{((from.x + to.x) / 2.0) + (half * (to.y - from.y)),
                               ((from.y + to.y) / 2.0) - (half * (to.x - from.x))};
    const Point2 middleOfArc = pointOfArc(arc, arc.startAngle + (arc.sweep / 2.0));
    EXPECT_LE(distance(middleOfArc, middleOfBulge),
              (2.0 * kRelativeArcError + 4.0 * kRelativePointError) * scale);
}

// Rule 3: the fillet is the circle of tangentCircles, with the sides of the
// picks, nearest to them. Returns whether the choice was free of ties.
bool checkChoice(const Call& call, const Fillet& result) {
    const auto all = tangentCircles(call.first, call.second, call.radius);
    EXPECT_TRUE(all.hasValue());
    if (!all.hasValue()) {
        return false;
    }
    const Side sideOfFirst = solid_space::geom2d::side(call.first, call.pickSecond).value();
    const Side sideOfSecond = solid_space::geom2d::side(call.second, call.pickFirst).value();
    std::vector<TangentCircle> onSides;
    for (const auto& circle : all.value().circles) {
        if (circle.sideOfFirst == sideOfFirst && circle.sideOfSecond == sideOfSecond) {
            onSides.push_back(circle);
        }
    }
    EXPECT_FALSE(onSides.empty());
    if (onSides.empty()) {
        return false;
    }
    const auto nearest = nearestOf(onSides, call.pickFirst, call.pickSecond);
    const auto& chosen = onSides[nearest.index];
    EXPECT_TRUE(sameBits(chosen.center, result.center));
    EXPECT_TRUE(sameBits(chosen.onFirst, result.onFirst));
    EXPECT_TRUE(sameBits(chosen.onSecond, result.onSecond));
    EXPECT_TRUE(sameBits(result.radius, call.radius));
    EXPECT_TRUE(sameBits(result.arc.radius, call.radius));
    EXPECT_TRUE(sameBits(result.arc.center, result.center));
    EXPECT_FALSE(result.semicircle);
    return !nearest.tie;
}

// Swapping the curves and the picks gives the same circle, with the arc
// reversed: the same points and stations, the sweep negated (the angle between
// the two directions is computed symmetrically), the bulge of the opposite
// sign.
void checkSwapped(const Call& call, const Fillet& result) {
    const auto swapped =
        fillet(call.second, call.pickSecond, call.first, call.pickFirst, call.radius);
    ASSERT_TRUE(swapped.hasValue());
    const auto& other = swapped.value();
    ASSERT_EQ(other.outcome, TangencyOutcome::found);
    EXPECT_TRUE(sameBits(other.center, result.center));
    EXPECT_TRUE(sameBits(other.onFirst, result.onSecond));
    EXPECT_TRUE(sameBits(other.onSecond, result.onFirst));
    EXPECT_TRUE(sameBits(other.arc.sweep, -result.arc.sweep));
    EXPECT_LT(other.bulge * result.bulge, 0.0);
    EXPECT_NEAR(other.bulge, -result.bulge, 4.0 * kRelativeArcError);
}

void checkFillets(Point2 origin, double limit) {
    Generator generator(origin);
    std::size_t found = 0;
    std::size_t swapped = 0;
    for (int i = 0; i < 500; ++i) {
        const Call call = generator.call(limit);
        const auto result =
            fillet(call.first, call.pickFirst, call.second, call.pickSecond, call.radius);
        ASSERT_TRUE(result.hasValue());
        const auto& value = result.value();
        if (value.outcome != TangencyOutcome::found || value.semicircle) {
            continue;
        }
        ++found;
        const bool free = checkChoice(call, value);
        checkArc(call, value);
        // Away from ties, coincident points and diameters.
        if (free && value.arc.sweep != 0.0 && std::fabs(value.arc.sweep) != std::numbers::pi) {
            ++swapped;
            checkSwapped(call, value);
        }
    }
    EXPECT_GT(found, 150U);
    EXPECT_GT(swapped, 100U);
}

// --- tangentCircleNear -------------------------------------------------------

TEST(TangentCircleNear, ChoosesTheNearestCircle) {
    Generator generator(Point2{0.0, 0.0});
    std::size_t found = 0;
    for (int i = 0; i < 300; ++i) {
        const Call call = generator.call(50.0);
        const auto result = tangentCircleNear(call.first, call.pickFirst, call.second,
                                              call.pickSecond, call.radius);
        const auto all = tangentCircles(call.first, call.second, call.radius);
        ASSERT_TRUE(result.hasValue());
        ASSERT_TRUE(all.hasValue());
        const ChosenCircle& chosen = result.value();
        if (all.value().infinitelyMany) {
            EXPECT_EQ(chosen.outcome, TangencyOutcome::infinitelyMany);
            continue;
        }
        if (all.value().circles.empty()) {
            EXPECT_EQ(chosen.outcome, TangencyOutcome::none);
            continue;
        }
        ++found;
        ASSERT_EQ(chosen.outcome, TangencyOutcome::found);
        const auto nearest = nearestOf(all.value().circles, call.pickFirst, call.pickSecond);
        EXPECT_TRUE(sameCircle(chosen.circle, all.value().circles[nearest.index]));
    }
    EXPECT_GT(found, 150U);
}

TEST(TangentCircleNear, ATieGoesToTheFirstCircle) {
    // Two perpendicular lines and picks at their corner: the four circles of
    // radius 1 are equally near; the first along the first line is centred at
    // (-1, -1) (tests/corpus has the same case, cerchio-vicino-parita).
    const auto result = tangentCircleNear(Segment{{0.0, 0.0}, {10.0, 0.0}}, Point2{0.0, 0.0},
                                          Segment{{0.0, 0.0}, {0.0, 10.0}}, Point2{0.0, 0.0}, 1.0);
    ASSERT_TRUE(result.hasValue());
    ASSERT_EQ(result.value().outcome, TangencyOutcome::found);
    EXPECT_EQ(result.value().circle.center.x, -1.0);
    EXPECT_EQ(result.value().circle.center.y, -1.0);
}

TEST(TangentCircleNear, Diagnoses) {
    const Segment line{{0.0, 0.0}, {1.0, 0.0}};
    const Circle circle{{0.0, 0.0}, 1.0};
    const Point2 pick{1.0, 1.0};
    const Point2 nan{std::nan(""), 0.0};
    const auto check = [](const auto& result, DiagnosisCode code, std::size_t input) {
        ASSERT_FALSE(result.hasValue());
        EXPECT_EQ(result.diagnosis().code, code);
        EXPECT_EQ(result.diagnosis().input, input);
    };
    // The curves come first, then the radius, then the picks.
    check(tangentCircleNear(Segment{{0.0, 0.0}, {0.0, 0.0}}, nan, circle, pick, -1.0),
          DiagnosisCode::degenerateCurve, 0U);
    check(tangentCircleNear(line, nan, Circle{{0.0, 0.0}, kInfinity}, pick, 1.0),
          DiagnosisCode::invalidValue, 1U);
    check(tangentCircleNear(line, nan, circle, pick, 0.0), DiagnosisCode::degenerateCurve, 2U);
    check(tangentCircleNear(line, pick, circle, pick, 0x1p51), DiagnosisCode::outOfDomain, 2U);
    check(tangentCircleNear(line, nan, circle, nan, 1.0), DiagnosisCode::invalidValue, 3U);
    check(tangentCircleNear(line, pick, circle, Point2{0.0, -0x1p51}, 1.0),
          DiagnosisCode::outOfDomain, 4U);
    // A centre beyond the domain, as tangentCircles says.
    check(tangentCircleNear(line, pick, Segment{{0.0, 1.0}, {1.0, 1.0 + 0x1p-52}}, pick, 0.25),
          DiagnosisCode::outOfDomain, 2U);
}

// --- fillet ------------------------------------------------------------------

TEST(Fillet, PropertiesNearTheOrigin) {
    checkFillets(Point2{0.0, 0.0}, 50.0);
}

TEST(Fillet, PropertiesFarAway) {
    checkFillets(Point2{1.0e6, -3.0e5}, 50.0);
}

TEST(Fillet, SemicircleBetweenSlantedPieces) {
    // Two parallel lines along (4, 3), at distance 10: the second is the first
    // moved by (-6, 8). The pick is nearer the start (0, 0) of the first piece,
    // whose foot on the second line is (-6, 8); the centre is (-3, 4), the
    // radius 5, and the semicircle leaves (0, 0) against the direction (4, 3)
    // of the first curve, so it turns clockwise from (0, 0) to (-6, 8).
    const PolylineSegment first{{0.0, 0.0}, {8.0, 6.0}, 0.0};
    const Segment second{{-2.0, 11.0}, {2.0, 14.0}};
    const auto result = fillet(first, Point2{1.0, 1.0}, second, Point2{0.0, 12.5}, 3.0);
    ASSERT_TRUE(result.hasValue());
    const auto& value = result.value();
    ASSERT_EQ(value.outcome, TangencyOutcome::found);
    EXPECT_TRUE(value.semicircle);
    EXPECT_EQ(value.radius, 5.0);
    EXPECT_EQ(value.center.x, -3.0);
    EXPECT_EQ(value.center.y, 4.0);
    EXPECT_EQ(value.onFirst.point.x, 0.0);
    EXPECT_EQ(value.onFirst.point.y, 0.0);
    EXPECT_EQ(value.onFirst.position.location, Location::atStart);
    EXPECT_EQ(value.onSecond.point.x, -6.0);
    EXPECT_EQ(value.onSecond.point.y, 8.0);
    EXPECT_EQ(value.onSecond.position.location, Location::beforeStart);
    EXPECT_EQ(value.onSecond.position.station, -5.0);
    EXPECT_EQ(value.arc.radius, 5.0);
    EXPECT_EQ(value.arc.sweep, -std::numbers::pi);
    EXPECT_EQ(value.bulge, -1.0);
    // The middle of the semicircle lies beyond the start, away from the first
    // curve: (-3, 4) - 5 (4, 3) / 5 = (-7, 1).
    const Point2 middle = pointOfArc(value.arc, value.arc.startAngle + (value.arc.sweep / 2.0));
    EXPECT_NEAR(middle.x, -7.0, 1e-13);
    EXPECT_NEAR(middle.y, 1.0, 1e-13);
}

TEST(Fillet, CornerNearestThePicks) {
    // A line through a circle of radius 5: the corners are (-5, 0) and (5, 0),
    // and the picks are nearer the second.
    const Segment line{{-10.0, 0.0}, {10.0, 0.0}};
    const Circle circle{{0.0, 0.0}, 5.0};
    const auto result = fillet(line, Point2{4.0, 1.0}, circle, Point2{6.0, -1.0}, 0.0);
    ASSERT_TRUE(result.hasValue());
    const auto& value = result.value();
    ASSERT_EQ(value.outcome, TangencyOutcome::found);
    EXPECT_EQ(value.center.x, 5.0);
    EXPECT_EQ(value.center.y, 0.0);
    EXPECT_EQ(value.radius, 0.0);
    EXPECT_EQ(value.onFirst.position.location, Location::inside);
    EXPECT_EQ(value.onSecond.position.location, Location::inside);
    EXPECT_EQ(value.arc.radius, 0.0);
    EXPECT_EQ(value.arc.sweep, 0.0);
    EXPECT_EQ(value.bulge, 0.0);
    // The corner is a common point that intersect() gives.
    const auto common =
        solid_space::geom2d::intersect(line, Extension::extended, circle, Extension::extended);
    ASSERT_TRUE(common.hasValue());
    ASSERT_EQ(common.value().points.size(), 2U);
    EXPECT_TRUE(sameBits(common.value().points[1].point, value.center));
    // The other picks choose the other corner; -0 is a radius of zero too.
    const auto other = fillet(line, Point2{-4.0, 1.0}, circle, Point2{-6.0, -1.0}, -0.0);
    ASSERT_TRUE(other.hasValue());
    ASSERT_EQ(other.value().outcome, TangencyOutcome::found);
    EXPECT_EQ(other.value().center.x, -5.0);
}

TEST(Fillet, TinyRadius) {
    // The corner of two perpendicular segments, at the scale 2^-600 where the
    // products of the coordinates underflow (review of #123): the fillet is
    // the clockwise quarter turn around (r, r) from (r, 0) to (0, r), sweep
    // -pi/2 and bulge tan(-pi/8) = 1 - sqrt(2). Scaling the data by a power of
    // two scales centre, radius and points exactly and keeps the bits of the
    // angles and of the bulge: the same fillet at scale 1 is the reference.
    for (const double r : {0x1p-600, 0x1p-1000, 1.0}) {
        const Call call{Segment{{0.0, 0.0}, {4.0 * r, 0.0}}, Point2{2.0 * r, 0.0},
                        Segment{{0.0, 0.0}, {0.0, 4.0 * r}}, Point2{0.0, 2.0 * r}, r};
        const auto result =
            fillet(call.first, call.pickFirst, call.second, call.pickSecond, call.radius);
        ASSERT_TRUE(result.hasValue());
        const auto& value = result.value();
        ASSERT_EQ(value.outcome, TangencyOutcome::found);
        EXPECT_FALSE(value.semicircle);
        EXPECT_EQ(value.center.x, r);
        EXPECT_EQ(value.center.y, r);
        EXPECT_EQ(value.onFirst.point.x, r);
        EXPECT_EQ(value.onFirst.point.y, 0.0);
        EXPECT_EQ(value.onSecond.point.x, 0.0);
        EXPECT_EQ(value.onSecond.point.y, r);
        EXPECT_NEAR(value.arc.startAngle, -std::numbers::pi / 2.0, 1e-15);
        EXPECT_NEAR(value.arc.sweep, -std::numbers::pi / 2.0, 1e-15);
        EXPECT_NEAR(value.bulge, 1.0 - std::numbers::sqrt2, 1e-15);
        checkArc(call, value);
        // The end of the Arc is the second point of tangency, at this scale.
        const Point2 end = pointOfArc(value.arc, value.arc.startAngle + value.arc.sweep);
        EXPECT_LE(distance(end, value.onSecond.point), 4.0 * kRelativeArcError * r);
        const auto unit = fillet(Segment{{0.0, 0.0}, {4.0, 0.0}}, Point2{2.0, 0.0},
                                 Segment{{0.0, 0.0}, {0.0, 4.0}}, Point2{0.0, 2.0}, 1.0);
        ASSERT_TRUE(unit.hasValue());
        EXPECT_TRUE(sameBits(value.arc.startAngle, unit.value().arc.startAngle));
        EXPECT_TRUE(sameBits(value.arc.sweep, unit.value().arc.sweep));
        EXPECT_TRUE(sameBits(value.bulge, unit.value().bulge));
    }
}

TEST(Fillet, BeyondTheDomain) {
    // Two almost parallel lines meet near x = -2^52: the corner, and the
    // centres of the tangent circles, lie beyond robust::kMaxCoordinate.
    const Segment first{{0.0, 0.0}, {1.0, 0.0}};
    const Segment second{{0.0, 1.0}, {1.0, 1.0 + 0x1p-52}};
    for (const double radius : {0.0, 0.25}) {
        const auto result = fillet(first, Point2{0.5, 0.0}, second, Point2{0.5, 1.0}, radius);
        ASSERT_FALSE(result.hasValue());
        EXPECT_EQ(result.diagnosis().code, DiagnosisCode::outOfDomain);
        EXPECT_EQ(result.diagnosis().input, 2U);
    }
}

TEST(Fillet, Diagnoses) {
    const Segment line{{0.0, 0.0}, {1.0, 0.0}};
    const Circle circle{{0.0, 0.0}, 1.0};
    const Point2 pick{1.0, 1.0};
    const Point2 nan{std::nan(""), 0.0};
    const auto check = [](const auto& result, DiagnosisCode code, std::size_t input) {
        ASSERT_FALSE(result.hasValue());
        EXPECT_EQ(result.diagnosis().code, code);
        EXPECT_EQ(result.diagnosis().input, input);
    };
    check(fillet(Circle{{0.0, 0.0}, 0.0}, nan, circle, pick, -1.0), DiagnosisCode::degenerateCurve,
          0U);
    check(fillet(line, nan, Segment{{0.0, 0.0}, {0.0, kInfinity}}, pick, 1.0),
          DiagnosisCode::invalidValue, 1U);
    check(fillet(line, nan, circle, pick, std::nan("")), DiagnosisCode::invalidValue, 2U);
    check(fillet(line, nan, circle, pick, 0x1p51), DiagnosisCode::outOfDomain, 2U);
    check(fillet(line, nan, circle, pick, 0.0), DiagnosisCode::invalidValue, 3U);
    check(fillet(line, Point2{0x1p51, 0.0}, circle, pick, 1.0), DiagnosisCode::outOfDomain, 3U);
    check(fillet(line, pick, circle, Point2{kInfinity, 0.0}, 1.0), DiagnosisCode::invalidValue, 4U);
}

}  // namespace
