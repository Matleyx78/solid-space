// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Unit tests of tangentCircleNear and fillet (stage 0.2, packages P16 and
// P22).
//
// The properties of the package sheet are checked on data generated
// deterministically (a fixed SplitMix64 sequence, with its own conversion to
// doubles, so that every system draws the same data), at two scales: the
// fillet is a circle of tangentCircles with the sides of rule 3, the nearest
// to the picks; the ends of its arc coincide with the points of tangency, also
// for the rule of ADR 0009; the Arc and the bulge describe the same arc;
// swapping the curves and the picks gives the same circle with the arc
// reversed. The parts kept (rule 5, package P22) are checked for every pair of
// kinds of curve: reversing a curve swaps its value and keeps the other,
// swapping the curves swaps the two fields, and with an arc to continue the
// path does not turn back. The exact expectations of the cases of the request
// and of AutoCAD are in tests/corpus/ ([0.2-R16a], [0.2-R16b], [0.2-R16c],
// [0.2-R22], [0.2-R22b]); the few hand-made cases here have their answer
// worked out in the comments.

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
#include <optional>
#include <variant>
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
using solid_space::geom2d::KeptPart;
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

    // The kinds of curve: segment, straight piece, circle, arc, curved piece.
    static constexpr std::size_t kKinds = 5;

    Curve curve(double limit) {
        const Point2 start = point(limit);
        const Point2 end = point(limit);
        return shaped(m_sequence.index(kKinds), start, end, limit);
    }

    Curve curveOfKind(std::size_t kind, double limit) {
        const Point2 start = point(limit);
        const Point2 end = point(limit);
        return shaped(kind, start, end, limit);
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

    // A call with curves of the given kinds.
    Call callOfKinds(std::size_t first, std::size_t second, double limit) {
        const Curve a = curveOfKind(first, limit);
        const Point2 pickFirst = point(limit);
        const Curve b = curveOfKind(second, limit);
        const Point2 pickSecond = point(limit);
        return Call{a, pickFirst, b, pickSecond, m_sequence.between(limit / 100.0, limit / 2.0)};
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
    Curve shaped(std::size_t kind, Point2 start, Point2 end, double limit) {
        if (start.x == end.x && start.y == end.y) {
            end.x = above(end.x);
        }
        switch (kind) {
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

// --- The parts kept (rule 5) --------------------------------------------------

KeptPart opposite(KeptPart part) {
    return part == KeptPart::fromStart ? KeptPart::toEnd : KeptPart::fromStart;
}

// The same curve run the other way, when its geometry stays the same bit for
// bit: an Arc only when the start angle of the reversed one, the rounded sum
// a + s, gives back a when s is subtracted (its D0 and D1 are then those of
// the original swapped). A Circle has no direction to reverse.
std::optional<Curve> reversed(const Curve& curve) {
    if (const auto* segment = std::get_if<Segment>(&curve)) {
        return Segment{segment->end, segment->start};
    }
    if (const auto* piece = std::get_if<PolylineSegment>(&curve)) {
        return PolylineSegment{piece->end, piece->start, -piece->bulge};
    }
    if (const auto* arc = std::get_if<Arc>(&curve)) {
        const double start = arc->startAngle + arc->sweep;
        if (start + (-arc->sweep) != arc->startAngle) {
            return std::nullopt;
        }
        return Arc{arc->center, arc->radius, start, -arc->sweep};
    }
    return std::nullopt;
}

Point2 quarterTurn(Point2 vector) {
    return Point2{-vector.y, vector.x};
}

Point2 scaled(Point2 vector, double factor) {
    return Point2{vector.x * factor, vector.y * factor};
}

Point2 minus(Point2 a, Point2 b) {
    return Point2{a.x - b.x, a.y - b.y};
}

double dotOf(Point2 a, Point2 b) {
    return (a.x * b.x) + (a.y * b.y);
}

// The direction of travel of a curve at a point of its carrier, in doubles:
// enough for the signs here, where it is parallel to a vector of the same
// length scale. The centre of a curved piece is m + (b^2 - 1) / (4 b) times the
// chord turned clockwise.
Point2 travelAt(const Curve& curve, Point2 at) {
    if (const auto* segment = std::get_if<Segment>(&curve)) {
        return minus(segment->end, segment->start);
    }
    if (const auto* circle = std::get_if<Circle>(&curve)) {
        return quarterTurn(minus(at, circle->center));
    }
    if (const auto* arc = std::get_if<Arc>(&curve)) {
        return scaled(quarterTurn(minus(at, arc->center)), arc->sweep > 0.0 ? 1.0 : -1.0);
    }
    const auto& piece = std::get<PolylineSegment>(curve);
    const Point2 chord = minus(piece.end, piece.start);
    if (piece.bulge == 0.0) {
        return chord;
    }
    const double b = piece.bulge;
    const double k = ((b * b) - 1.0) / (4.0 * b);
    const Point2 centre{((piece.start.x + piece.end.x) / 2.0) + (k * chord.y),
                        ((piece.start.y + piece.end.y) / 2.0) - (k * chord.x)};
    return scaled(quarterTurn(minus(at, centre)), b > 0.0 ? 1.0 : -1.0);
}

bool isStraight(const Curve& curve) {
    if (std::holds_alternative<Segment>(curve)) {
        return true;
    }
    const auto* piece = std::get_if<PolylineSegment>(&curve);
    return piece != nullptr && piece->bulge == 0.0;
}

// With an arc to continue the path does not turn back: the kept part of the
// first curve runs into its point of tangency in the direction in which the
// arc leaves it, and the arc reaches the second point in the direction in
// which the kept part of the second curve leaves it. A straight curve keeps a
// part that contains its point of tangency.
void checkContinues(const Call& call, const Fillet& result) {
    const double turn = result.arc.sweep > 0.0 ? 1.0 : -1.0;
    const Point2 first = result.onFirst.point;
    const Point2 second = result.onSecond.point;
    const Point2 leaving = scaled(quarterTurn(minus(first, result.center)), turn);
    const Point2 reaching = scaled(quarterTurn(minus(second, result.center)), turn);
    const Point2 alongFirst = travelAt(call.first, first);
    const Point2 alongSecond = travelAt(call.second, second);
    const Point2 intoFirst =
        result.keepFirst == KeptPart::fromStart ? alongFirst : scaled(alongFirst, -1.0);
    const Point2 outOfSecond =
        result.keepSecond == KeptPart::toEnd ? alongSecond : scaled(alongSecond, -1.0);
    EXPECT_GT(dotOf(intoFirst, leaving), 0.0);
    EXPECT_GT(dotOf(reaching, outOfSecond), 0.0);
    const auto keepsItsPoint = [](const Curve& curve, Location location, KeptPart part) {
        if (!isStraight(curve)) {
            return true;
        }
        const bool turnsBack = (location == Location::beforeStart && part == KeptPart::fromStart) ||
                               (location == Location::afterEnd && part == KeptPart::toEnd);
        return !turnsBack;
    };
    EXPECT_TRUE(keepsItsPoint(call.first, result.onFirst.position.location, result.keepFirst));
    EXPECT_TRUE(keepsItsPoint(call.second, result.onSecond.position.location, result.keepSecond));
}

bool sameGeometry(const Fillet& a, const Fillet& b) {
    return a.semicircle == b.semicircle && sameBits(a.center, b.center) &&
           sameBits(a.radius, b.radius) && sameBits(a.onFirst.point, b.onFirst.point) &&
           sameBits(a.onSecond.point, b.onSecond.point);
}

struct KeptCounts {
    std::size_t found = 0;
    std::size_t continued = 0;
    std::size_t tooLarge = 0;
    std::size_t reversed = 0;
    std::size_t swapped = 0;
};

Fillet filletOf(const Call& call) {
    const auto result =
        fillet(call.first, call.pickFirst, call.second, call.pickSecond, call.radius);
    EXPECT_TRUE(result.hasValue());
    return result.hasValue() ? result.value() : Fillet{};
}

// Reversing one curve, when the same geometry is chosen, swaps its value and
// keeps the other; swapping the curves swaps the two fields.
void checkKeptParts(const Call& call, KeptCounts& counts) {
    const Fillet value = filletOf(call);
    if (value.outcome == TangencyOutcome::radiusTooLarge) {
        ++counts.tooLarge;
        return;
    }
    if (value.outcome != TangencyOutcome::found) {
        return;
    }
    ++counts.found;
    if (value.arc.sweep != 0.0) {
        ++counts.continued;
        checkContinues(call, value);
    }
    for (const bool ofFirst : {true, false}) {
        const auto other = reversed(ofFirst ? call.first : call.second);
        if (!other.has_value()) {
            continue;
        }
        Call changed = call;
        (ofFirst ? changed.first : changed.second) = *other;
        const Fillet again = filletOf(changed);
        if (again.outcome != TangencyOutcome::found || !sameGeometry(again, value)) {
            continue;
        }
        ++counts.reversed;
        EXPECT_EQ(again.keepFirst, ofFirst ? opposite(value.keepFirst) : value.keepFirst);
        EXPECT_EQ(again.keepSecond, ofFirst ? value.keepSecond : opposite(value.keepSecond));
    }
    const Fillet swapped =
        filletOf(Call{call.second, call.pickSecond, call.first, call.pickFirst, call.radius});
    if (swapped.outcome == TangencyOutcome::found && swapped.semicircle == value.semicircle &&
        sameBits(swapped.center, value.center) &&
        sameBits(swapped.onFirst.point, value.onSecond.point) &&
        sameBits(swapped.onSecond.point, value.onFirst.point)) {
        ++counts.swapped;
        EXPECT_EQ(swapped.keepFirst, value.keepSecond);
        EXPECT_EQ(swapped.keepSecond, value.keepFirst);
    }
}

// Every pair of kinds of curve, with a radius and with radius zero (rule 2).
void checkKeptPartsOfKinds(Point2 origin, double limit) {
    Generator generator(origin);
    KeptCounts counts;
    KeptCounts corners;
    for (std::size_t first = 0; first < Generator::kKinds; ++first) {
        for (std::size_t second = 0; second < Generator::kKinds; ++second) {
            for (int i = 0; i < 40; ++i) {
                Call call = generator.callOfKinds(first, second, limit);
                checkKeptParts(call, counts);
                call.radius = 0.0;
                checkKeptParts(call, corners);
            }
        }
    }
    EXPECT_GT(counts.continued, 300U);
    EXPECT_GT(counts.tooLarge, 100U);
    EXPECT_GT(counts.reversed, 450U);
    EXPECT_GT(counts.swapped, 300U);
    EXPECT_GT(corners.found, 300U);
    EXPECT_GT(corners.reversed, 500U);
    EXPECT_GT(corners.swapped, 300U);
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
    // Rule 5 without an arc: the picks lie before the corner (5, 0) on the
    // line, (-1, 1) . (20, 0) < 0, and on the counterclockwise circle,
    // (1, -1) . (0, 5) < 0.
    EXPECT_EQ(value.keepFirst, KeptPart::fromStart);
    EXPECT_EQ(value.keepSecond, KeptPart::fromStart);
    // The other picks choose the other corner; -0 is a radius of zero too.
    const auto other = fillet(line, Point2{-4.0, 1.0}, circle, Point2{-6.0, -1.0}, -0.0);
    ASSERT_TRUE(other.hasValue());
    ASSERT_EQ(other.value().outcome, TangencyOutcome::found);
    EXPECT_EQ(other.value().center.x, -5.0);
    // At (-5, 0): (1, 1) . (20, 0) > 0 and (-1, -1) . (0, -5) > 0.
    EXPECT_EQ(other.value().keepFirst, KeptPart::toEnd);
    EXPECT_EQ(other.value().keepSecond, KeptPart::toEnd);
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

// Decision #149, with K = robust::kMaxCoordinate = 2^50. The circle of radius 1
// around (K - 1/2, 0) and the line x = K - 1/2, r = 1/2: every centre lies
// within the domain, but the circle centred at (K, 0) touches the given circle
// at (K + 1/2, 0), beyond it. tangentCircleNear and fillet inherit the
// diagnosis of the whole call (#151), even when the picks would choose
// another circle.
TEST(Fillet, PointOfTangencyBeyondTheDomain) {
    constexpr double kLimit = solid_space::robust::kMaxCoordinate;
    const Circle circle{{kLimit - 0.5, 0.0}, 1.0};
    const Segment line{{kLimit - 0.5, -1.0}, {kLimit - 0.5, 1.0}};
    const Point2 pick{kLimit - 10.0, 5.0};
    const auto near = tangentCircleNear(circle, pick, line, pick, 0.5);
    ASSERT_FALSE(near.hasValue());
    EXPECT_EQ(near.diagnosis().code, DiagnosisCode::outOfDomain);
    EXPECT_EQ(near.diagnosis().input, 2U);
    const auto result = fillet(circle, pick, line, pick, 0.5);
    ASSERT_FALSE(result.hasValue());
    EXPECT_EQ(result.diagnosis().code, DiagnosisCode::outOfDomain);
    EXPECT_EQ(result.diagnosis().input, 2U);
}

// Rule 1 checks its constructed radius on the exact value. The lines
// x + y = -(2K - 2) and x + y = 2K - 2 are (4K - 4) / sqrt(2) apart: the
// corner E = (2 - K, -K), the foot (K, K - 2) and the centre (1, -1) are
// within the domain, but the radius (2K - 2) / sqrt(2), about 1.41 K, is not.
TEST(Fillet, SemicircleRadiusBeyondTheDomain) {
    constexpr double kLimit = solid_space::robust::kMaxCoordinate;
    const Segment first{{-kLimit, 2.0 - kLimit}, {2.0 - kLimit, -kLimit}};
    const Segment second{{kLimit - 2.0, kLimit}, {kLimit, kLimit - 2.0}};
    const auto result =
        fillet(first, Point2{2.0 - kLimit, -kLimit}, second, Point2{kLimit, kLimit - 2.0}, 1.0);
    ASSERT_FALSE(result.hasValue());
    EXPECT_EQ(result.diagnosis().code, DiagnosisCode::outOfDomain);
    EXPECT_EQ(result.diagnosis().input, 2U);
}

// Rule 1 checks the point m of its bulge form, reconstructed from the returned
// doubles. The lines y = 0 and y = 200, with the corner E = (K - c, 0) at the
// end of the first piece: the foot is (K - c, 200), the centre (K - c, 100) and
// the radius 100, all exact doubles; the semicircle leaves E away from the
// first piece, in +x, so m = (K - c + 100, 100) exactly. With c = 10 it is
// beyond the domain, far beyond the band of #157 (kRelativeArcError * K = 16);
// with c = 200 it is within it.
TEST(Fillet, SemicircleMiddleBeyondTheDomain) {
    constexpr double kLimit = solid_space::robust::kMaxCoordinate;
    const auto semicircle = [&](double c) {
        const Segment first{{kLimit - 1000.0, 0.0}, {kLimit - c, 0.0}};
        const Segment second{{kLimit - 1000.0, 200.0}, {kLimit - c, 200.0}};
        return fillet(first, Point2{kLimit - c - 10.0, 0.0}, second,
                      Point2{kLimit - c - 10.0, 200.0}, 1.0);
    };
    const auto beyond = semicircle(10.0);
    ASSERT_FALSE(beyond.hasValue());
    EXPECT_EQ(beyond.diagnosis().code, DiagnosisCode::outOfDomain);
    EXPECT_EQ(beyond.diagnosis().input, 2U);
    const auto within = semicircle(200.0);
    ASSERT_TRUE(within.hasValue());
    const auto& value = within.value();
    EXPECT_EQ(value.outcome, TangencyOutcome::found);
    EXPECT_TRUE(value.semicircle);
    EXPECT_TRUE(sameBits(value.center, Point2{kLimit - 200.0, 100.0}));
    EXPECT_TRUE(sameBits(value.onSecond.point, Point2{kLimit - 200.0, 200.0}));
    EXPECT_TRUE(sameBits(value.radius, 100.0));
    EXPECT_TRUE(sameBits(std::fabs(value.bulge), 1.0));
}

// Rule 3 checks the point m of its bulge form too. The line y = 0 and the
// circle of radius 1 around (K - 80, 1998), r = 1000: the centres lie on
// y = 1000 at K - 80 +- sqrt(5997) (outside the circle) and K - 80 +-
// sqrt(1997) (enclosing it), all within the domain, and so do their points of
// tangency, so tangentCircleNear succeeds. The fillet near the picks is centred
// at about (K - 157.4, 1000): its arc runs counterclockwise from (K - 157.4, 0)
// to the top of the small circle, through its point at angle zero, and its
// middle m is near (K + 842, 962), far beyond the domain.
TEST(Fillet, ArcMiddleBeyondTheDomain) {
    constexpr double kLimit = solid_space::robust::kMaxCoordinate;
    const Segment line{{kLimit - 1000.0, 0.0}, {kLimit, 0.0}};
    const Circle circle{{kLimit - 80.0, 1998.0}, 1.0};
    const Point2 pickFirst{kLimit - 160.0, 0.0};
    const Point2 pickSecond{kLimit - 80.0, 1990.0};
    const auto near = tangentCircleNear(line, pickFirst, circle, pickSecond, 1000.0);
    ASSERT_TRUE(near.hasValue());
    ASSERT_EQ(near.value().outcome, TangencyOutcome::found);
    EXPECT_LT(near.value().circle.center.x, kLimit - 150.0);
    const auto result = fillet(line, pickFirst, circle, pickSecond, 1000.0);
    ASSERT_FALSE(result.hasValue());
    EXPECT_EQ(result.diagnosis().code, DiagnosisCode::outOfDomain);
    EXPECT_EQ(result.diagnosis().input, 2U);
}

// Decision #152: when the returned points of tangency coincide the bulge is 0,
// while the Arc keeps its own rules. Rule 1 (found in the review of #180): the
// lines y = x and y = x + d, d = 2^-55, are distinct and parallel; the corner
// is (1, 1) and the exact foot (1 - d/2, 1 + d/2), which rounds to (1, 1). The
// Arc keeps its half turn.
TEST(Fillet, SemicircleWithCoincidentPoints) {
    constexpr double kGap = 0x1p-55;
    const Segment first{{0.0, 0.0}, {1.0, 1.0}};
    const Segment second{{0.0, kGap}, {0.125, 0.125 + kGap}};
    const auto result = fillet(first, Point2{1.0, 1.0}, second, Point2{0.0, kGap}, 1.0);
    ASSERT_TRUE(result.hasValue());
    const auto& value = result.value();
    ASSERT_EQ(value.outcome, TangencyOutcome::found);
    EXPECT_TRUE(value.semicircle);
    ASSERT_TRUE(sameBits(value.onFirst.point, Point2{1.0, 1.0}));
    ASSERT_TRUE(sameBits(value.onSecond.point, Point2{1.0, 1.0}));
    EXPECT_TRUE(sameBits(value.bulge, 0.0));
    EXPECT_EQ(std::fabs(value.arc.sweep), std::numbers::pi);
}

// The same in rule 3, when the exact points of tangency are opposite ends of a
// diameter (rule 4: the counterclockwise half turn). The line from (0, 0) to
// (3, 4) has the unit normal n = (-4, 3) / 5 on its left. With T1 = (3/4, 1),
// r = 2^-60 and R = 5 * 2^-10 - 2^-59, the circle of radius R around
// T1 + (2r + R) n = (3/4 - 2^-8, 1 + 3 * 2^-10) is touched at T2 = T1 + 2r n by
// the circle of radius r around T1 + r n, the only candidate on the sides of
// the picks. T2 is within 2^-59 of T1, so both round to (3/4, 1).
TEST(Fillet, HalfTurnWithCoincidentPoints) {
    constexpr double kRadius = 0x1p-60;
    const Segment line{{0.0, 0.0}, {3.0, 4.0}};
    const Circle circle{{0.75 - 0x1p-8, 1.0 + (3.0 * 0x1p-10)}, (5.0 * 0x1p-10) - 0x1p-59};
    const Point2 tangency{0.75, 1.0};
    const auto result = fillet(line, tangency, circle, circle.center, kRadius);
    ASSERT_TRUE(result.hasValue());
    const auto& value = result.value();
    ASSERT_EQ(value.outcome, TangencyOutcome::found);
    EXPECT_FALSE(value.semicircle);
    ASSERT_TRUE(sameBits(value.onFirst.point, tangency));
    ASSERT_TRUE(sameBits(value.onSecond.point, tangency));
    EXPECT_TRUE(sameBits(value.bulge, 0.0));
    EXPECT_TRUE(sameBits(value.arc.radius, kRadius));
    EXPECT_EQ(value.arc.sweep, std::numbers::pi);
}

// --- The parts kept (rule 5) --------------------------------------------------

TEST(Fillet, KeptPartsOnEveryPairOfKinds) {
    checkKeptPartsOfKinds(Point2{0.0, 0.0}, 50.0);
}

TEST(Fillet, KeptPartsOnEveryPairOfKindsFarAway) {
    checkKeptPartsOfKinds(Point2{1.0e6, -3.0e5}, 50.0);
}

void expectParts(const Call& call, KeptPart first, KeptPart second) {
    const Fillet value = filletOf(call);
    ASSERT_EQ(value.outcome, TangencyOutcome::found);
    EXPECT_EQ(value.keepFirst, first);
    EXPECT_EQ(value.keepSecond, second);
}

void expectOutcome(const Call& call, TangencyOutcome outcome) {
    const Fillet value = filletOf(call);
    EXPECT_EQ(value.outcome, outcome);
}

// The examples of #184. Two segments from the origin along the axes, r = 10:
// the centre is (10, 10) and the clockwise arc leaves (10, 0) in -x and
// reaches (0, 10) in -y; the first segment keeps (10, 0) -> (30, 0), the
// second (0, 10) -> (0, 30). The arc and the line of autocad-raccordi.dxf,
// r = 3: the circle of radius 13 around (230, 0) meets y = -2 at the centre
// (230 + sqrt(165), -2); the point of tangency on the arc is at angle
// -asin(2/13), before its start, and the arc keeps it with the part toward
// its end (the start moves back); on the line the fillet continues toward
// x = 250.
TEST(Fillet, KeptPartsOfTheRequest) {
    expectParts(Call{Segment{{0.0, 0.0}, {30.0, 0.0}}, Point2{5.0, 0.0},
                     Segment{{0.0, 0.0}, {0.0, 30.0}}, Point2{0.0, 5.0}, 10.0},
                KeptPart::toEnd, KeptPart::toEnd);
    expectParts(Call{Arc{{230.0, 0.0}, 10.0, 0.0, std::numbers::pi}, Point2{238.6603, 5.0},
                     Segment{{215.0, -5.0}, {250.0, -5.0}}, Point2{245.0, -5.0}, 3.0},
                KeptPart::toEnd, KeptPart::toEnd);
}

// #186. The lines y = 0 and y = 4 from x = 0 to x = 12, picks nearer x = 12:
// the semicircle around (12, 2) leaves (12, 0) in +x, away from the first
// curve, and leaves (12, 4) toward it in +x as well; both keep the part from
// their start, and reversing the second keeps the part toward its end. A line
// and a circle touched at the ends of a diameter: the counterclockwise half
// turn of rule 4 around (0, 1) leaves (0, 0) in +x and reaches (0, 2) in -x;
// swapped, it leaves (0, 2) in -x and reaches (0, 0) in +x.
TEST(Fillet, KeptPartsOfHalfTurns) {
    const Segment lower{{0.0, 0.0}, {12.0, 0.0}};
    const Segment upper{{0.0, 4.0}, {12.0, 4.0}};
    const Segment upperBack{{12.0, 4.0}, {0.0, 4.0}};
    expectParts(Call{lower, Point2{10.0, 0.0}, upper, Point2{10.0, 4.0}, 1.0}, KeptPart::fromStart,
                KeptPart::fromStart);
    expectParts(Call{lower, Point2{10.0, 0.0}, upperBack, Point2{10.0, 4.0}, 1.0},
                KeptPart::fromStart, KeptPart::toEnd);
    const Segment line{{-10.0, 0.0}, {10.0, 0.0}};
    const Circle circle{{0.0, 3.0}, 1.0};
    expectParts(Call{line, Point2{0.0, 0.0}, circle, Point2{0.0, 2.0}, 1.0}, KeptPart::fromStart,
                KeptPart::fromStart);
    expectParts(Call{circle, Point2{0.0, 2.0}, line, Point2{0.0, 0.0}, 1.0}, KeptPart::toEnd,
                KeptPart::toEnd);
}

// #187. The first segment runs from x = 2 to x = 5 on y = 0, the second up
// the y axis, r = 10: the centre is (10, 10) and the first point of tangency
// (10, 0) lies beyond the end of the first segment, but the clockwise arc
// leaves it in -x, so the first segment would have to turn back there. The
// same reversed: the point lies before its start and the part kept would be
// the one from the start. A longer first segment makes the fillet.
TEST(Fillet, RadiusTooLargeWhenALineWouldTurnBack) {
    const Segment axis{{0.0, 0.0}, {0.0, 30.0}};
    for (const Segment& first :
         {Segment{{2.0, 0.0}, {5.0, 0.0}}, Segment{{5.0, 0.0}, {2.0, 0.0}}}) {
        const auto result = fillet(first, Point2{3.0, 0.0}, axis, Point2{0.0, 20.0}, 10.0);
        ASSERT_TRUE(result.hasValue());
        const auto& value = result.value();
        EXPECT_EQ(value.outcome, TangencyOutcome::radiusTooLarge);
        EXPECT_EQ(value.radius, 0.0);
        EXPECT_FALSE(value.semicircle);
        EXPECT_EQ(value.bulge, 0.0);
    }
    expectParts(
        Call{Segment{{2.0, 0.0}, {15.0, 0.0}}, Point2{3.0, 0.0}, axis, Point2{0.0, 20.0}, 10.0},
        KeptPart::toEnd, KeptPart::toEnd);
}

// #188. The counterclockwise quarter of the unit circle from angle 0 to pi/2
// and the line y = -1, r = 0: the corner (0, -1) is at angle 3 pi/2 from the
// start, beyond the end. A pick at angle pi/4 lies in both parts: extending
// the end costs pi, moving the start back pi/2, so the part toward the end.
// A pick at angle pi lies only in the part from the start, one at about
// 5 pi/3 (beyond the corner) only in the part toward the end. On the line the picks lie after the
// corner. The arc reversed (start pi/2, sweep -pi/2, which gives back 0
// exactly) swaps the value of the arc.
TEST(Fillet, ArcExtendedTheShorterWay) {
    const Arc quarter{{0.0, 0.0}, 1.0, 0.0, std::numbers::pi / 2.0};
    const Arc back{{0.0, 0.0}, 1.0, std::numbers::pi / 2.0, -std::numbers::pi / 2.0};
    const Segment line{{-2.0, -1.0}, {2.0, -1.0}};
    const Point2 onLine{1.0, -1.0};
    expectParts(Call{quarter, Point2{0.5, 0.5}, line, onLine, 0.0}, KeptPart::toEnd,
                KeptPart::toEnd);
    expectParts(Call{quarter, Point2{-1.0, 0.0}, line, onLine, 0.0}, KeptPart::fromStart,
                KeptPart::toEnd);
    expectParts(Call{quarter, Point2{0.5, -0.9}, line, onLine, 0.0}, KeptPart::toEnd,
                KeptPart::toEnd);
    expectParts(Call{back, Point2{0.5, 0.5}, line, onLine, 0.0}, KeptPart::fromStart,
                KeptPart::toEnd);
    expectParts(Call{back, Point2{-1.0, 0.0}, line, onLine, 0.0}, KeptPart::toEnd, KeptPart::toEnd);
}

// #188, exact parity. The counterclockwise half of the circle of radius 2
// from (0, -2) through (2, 0) to (0, 2), and the line x = -2, r = 0: the corner
// (-2, 0) is at angle 3 pi/2 from the start, pi/2 beyond the end and pi/2
// before the start. The pick at (2, 0) lies on the given arc, in both parts:
// the two extensions are equal, and no part is shown. A pick at the centre of
// the arc has no direction.
TEST(Fillet, EqualExtensionsAreAmbiguous) {
    const PolylineSegment half{{0.0, -2.0}, {0.0, 2.0}, 1.0};
    const Segment line{{-2.0, -3.0}, {-2.0, 3.0}};
    expectOutcome(Call{half, Point2{2.0, 0.0}, line, Point2{-2.0, 1.0}, 0.0},
                  TangencyOutcome::ambiguousPick);
    expectOutcome(Call{half, Point2{0.0, 0.0}, line, Point2{-2.0, 1.0}, 0.0},
                  TangencyOutcome::ambiguousPick);
    // At angle 5 pi/4 from the start, between the end and the corner, the pick
    // lies only in the part from the start.
    expectParts(Call{half, Point2{-1.0, 1.0}, line, Point2{-2.0, 1.0}, 0.0}, KeptPart::fromStart,
                KeptPart::toEnd);
}

// Without an arc, on lines: the segments y = x and y = 4 - x cross at (2, 2).
// The pick (3, 1) lies on the normal to the first at the corner, so it shows
// no part; one ulp to the right it shows the part toward the end, one ulp to
// the left the part from the start. On the second, (3, 1) lies after the
// corner.
TEST(Fillet, PickOnTheNormalOfACorner) {
    const Segment rising{{0.0, 0.0}, {4.0, 4.0}};
    const Segment falling{{0.0, 4.0}, {4.0, 0.0}};
    const Point2 onFalling{3.0, 1.0};
    expectOutcome(Call{rising, Point2{3.0, 1.0}, falling, onFalling, 0.0},
                  TangencyOutcome::ambiguousPick);
    expectParts(Call{rising, Point2{std::nextafter(3.0, kInfinity), 1.0}, falling, onFalling, 0.0},
                KeptPart::toEnd, KeptPart::toEnd);
    expectParts(Call{rising, Point2{std::nextafter(3.0, 0.0), 1.0}, falling, onFalling, 0.0},
                KeptPart::fromStart, KeptPart::toEnd);
}

// The domain comes before the pick. With A = 2^50 - 2 and B = 2^49 - 3 the
// lines y = x - A + B and y = A + B + 6 - x cross at (A + 3, B + 3), beyond
// the domain; the pick (A + 2, B + 4) would lie on the normal to the first.
TEST(Fillet, DomainBeforeThePick) {
    constexpr double kA = 0x1p50 - 2.0;
    constexpr double kB = 0x1p49 - 3.0;
    const Segment first{{kA, kB}, {kA + 1.0, kB + 1.0}};
    const Segment second{{kA, kB + 6.0}, {kA + 1.0, kB + 5.0}};
    const auto result =
        fillet(first, Point2{kA + 2.0, kB + 4.0}, second, Point2{kA + 1.0, kB + 5.0}, 0.0);
    ASSERT_FALSE(result.hasValue());
    EXPECT_EQ(result.diagnosis().code, DiagnosisCode::outOfDomain);
    EXPECT_EQ(result.diagnosis().input, 2U);
}

// Rule 3 with the same exact point of tangency twice ([0.2-R22b]): the line
// y = 1 touches the unit circle at (0, 1), and so does the circle of radius
// 1/2 around (0, 1/2). There is no arc, and the picks show the parts with
// respect to (0, 1): on the normal and at the centre they show none; the picks
// (1/4, 1/2) and (-1/2, 0), on the same sides, lie after it on the line and
// on the counterclockwise circle.
TEST(Fillet, CoincidentTangenciesUseThePicks) {
    const Segment line{{-10.0, 1.0}, {10.0, 1.0}};
    const Circle circle{{0.0, 0.0}, 1.0};
    expectOutcome(Call{line, Point2{0.0, 0.5}, circle, Point2{0.0, 0.0}, 0.5},
                  TangencyOutcome::ambiguousPick);
    expectParts(Call{line, Point2{0.25, 0.5}, circle, Point2{-0.5, 0.0}, 0.5}, KeptPart::toEnd,
                KeptPart::toEnd);
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
