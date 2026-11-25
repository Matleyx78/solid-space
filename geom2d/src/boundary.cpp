// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// detectBoundary (boundary.hpp; ADR 0014; stage 0.3, package PS9): first the
// rule of steps 1-4 (boundary_rule.hpp, in namespace detail), then steps 5-7
// on the arrangement (arrangement.hpp) of the data of steps 1-3, and the
// construction of the returned loops.
//
// Every decision is exact: the face of the point, the loops, the depths, the
// order of the loops and of their pieces, the first vertex of a polyline. The
// middle of a stretch split in two is a point with nested radicals; it is
// compared with the other vertices in a tower of square roots
// (exact/tower.hpp). Only the returned doubles are rounded:
// - a vertex that is an end of the data of step 1 is copied; any other vertex
//   is rounded correctly (positions.hpp, nearestPoint), or for the middle of a
//   split stretch built from the rounded ends (boundary_rule.hpp,
//   stretchMiddle), within a few units of 2^-53 * max(|p|, |centre|, r);
// - a bulge comes from the rounded ends and the rounded centre and radius
//   (boundary_rule.hpp, arcBulge): the segment moves by a few units of
//   2^-53 * max(|p|, |centre|, r) from the exact stretch.
// The centre and radius of a stretch are within 3 M of the origin (curves.hpp,
// magnitude: a circle of a stretch above half a turn has a radius below the
// distance from its middle to the chord), so both errors are far below
// kRelativePointError = 2^-48 and kRelativeLoopError = 2^-44 relative to
// max(M, |p|). A converted Arc adds its own kRelativeLoopError / 2.

#include "solid_space/geom2d/boundary.hpp"

#include "arrangement.hpp"
#include "boundary_rule.hpp"
#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "exact/interval.hpp"
#include "exact/quadratic.hpp"
#include "exact/tower.hpp"
#include "positions.hpp"
#include "solid_space/geom2d/intersection.hpp"
#include "solid_space/robust/point.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace solid_space::geom2d::detail {
namespace {

bool isNegative(Sign sign) noexcept {
    return sign == Sign::negative;
}

bool isPositive(Sign sign) noexcept {
    return sign == Sign::positive;
}

Sign negated(Sign sign) noexcept {
    if (sign == Sign::positive) {
        return Sign::negative;
    }
    return sign == Sign::negative ? Sign::positive : Sign::zero;
}

Dyadic square(const Dyadic& value) {
    return value * value;
}

// True when two points of doubles are the same point (+0 and -0 are).
bool samePoints(Point2 a, Point2 b) {
    return compare(Dyadic(a.x), Dyadic(b.x)) == Sign::zero &&
           compare(Dyadic(a.y), Dyadic(b.y)) == Sign::zero;
}

// |a - b|, scaled so that no square overflows or underflows: a few units of
// 2^-53 relative.
double distance(Point2 a, Point2 b) {
    const double dx = std::fabs(a.x - b.x);
    const double dy = std::fabs(a.y - b.y);
    const double large = std::max(dx, dy);
    if (!(large > 0.0)) {
        return 0.0;
    }
    const double ratio = std::min(dx, dy) / large;
    return large * std::sqrt(1.0 + ratio * ratio);
}

// True when every point of the carrying circle has its coordinates within
// robust::kMaxCoordinate: |centre| + radius on each axis, exactly (as
// region_parts.cpp checks the contours of nestContours).
bool circleFits(const CircleSupport& circle) {
    const Dyadic limit = Dyadic(robust::kMaxCoordinate) * circle.weight;
    const Dyadic& centreX = circle.centerX;
    const Dyadic& centreY = circle.centerY;
    const Dyadic largest =
        compare(centreX.abs(), centreY.abs()) == Sign::positive ? centreX.abs() : centreY.abs();
    const Dyadic room = limit - largest;
    return room.sign() != Sign::negative &&
           compare(square(room), circle.radiusSquared) != Sign::negative;
}

// True when the exact distance between two points of doubles is at most eps.
bool within(Point2 a, Point2 b, double eps) {
    const Dyadic dx = Dyadic(a.x) - Dyadic(b.x);
    const Dyadic dy = Dyadic(a.y) - Dyadic(b.y);
    return compare(square(dx) + square(dy), square(Dyadic(eps))) != Sign::positive;
}

// True when the exact distance from a point of doubles to an exact point
// p = (a + b sqrt(d)) / w is at most eps. With u = w q - a:
//     |q - p|^2 w^2 = |u|^2 + |b|^2 d - 2 (u . b) sqrt(d).
bool within(Point2 q, const ExactPoint& p, double eps) {
    const Dyadic& w = p.weight;
    const Dyadic u = w * Dyadic(q.x) - p.rational.x;
    const Dyadic v = w * Dyadic(q.y) - p.rational.y;
    const Dyadic& bx = p.radical.x;
    const Dyadic& by = p.radical.y;
    const Dyadic rational =
        square(Dyadic(eps) * w) - square(u) - square(v) - (square(bx) + square(by)) * p.radicand;
    const Dyadic radical = (u * bx + v * by).timesPowerOfTwo(1);
    return exact::signOf(rational, radical, p.radicand) != Sign::negative;
}

// The sign of (q - p) . direction, for an exact point p:
//     ((w q - a) . direction - (b . direction) sqrt(d)) / w.
Sign alongSign(const ExactVector& direction, const ExactPoint& p, Point2 q) {
    const Dyadic u = p.weight * Dyadic(q.x) - p.rational.x;
    const Dyadic v = p.weight * Dyadic(q.y) - p.rational.y;
    const Dyadic rational = u * direction.x + v * direction.y;
    const Dyadic radical = -(p.radical.x * direction.x + p.radical.y * direction.y);
    return exact::signOf(rational, radical, p.radicand);
}

// --- Rounded points of an Arc, with a filter (stage 0.3 corrections, FS3) -----
//
// convertedArc() rounds the ends of an Arc as endpoints() does (curves.cpp,
// pointInDirection: exact::quadraticToDouble of c + r D / |D|, with c the
// centre and r the radius of the Arc and D an exact direction, D0 or D1), and
// its middle as nearestPoint() does (positions.hpp: the correctly rounded
// value of the same kind of point, since the circle of the meaning of an Arc
// has weight 1, centre c and squared radius r^2). Each coordinate is then the
// double nearest to a number Y, ties to even, where Y is X = c + r D / |D|
// itself (nearestPoint) or a number within 2^-125 |X| of it
// (quadraticToDouble: its square root S has 128 bits, so its sum
// (a + b S) / c, whose terms have the same sign, or its conjugate quotient,
// whose denominator adds terms of the same sign, is within 2^-126 |X| of X,
// and quotientToDouble() rounds it to nearest, ties to even). The exact
// numbers are long; the filter below decides most coordinates in doubles,
// with the intervals of exact/interval.hpp.
//
// The filter. T = r D / |D| lies in the interval [t - rT, t + rT]. With
// s = fl(c + t), the error e = (c + t) - s is exact (Knuth's TwoSum, exact in
// round to nearest when nothing overflows: |c|, |t| and |s| are at most
// 2^1000 here). So |X - s| <= |e| + rT, and every Y within 2^-120 |X| of X
// has |Y - s| <= |e| + rT + 2^-119 |s|, since |X| <= 2 |s| whenever the test
// below passes. The bound b = fl(fl(fl(|e| + rT) * (1 + 2^-40)) + |s| 2^-110)
// is above that: each rounding of a sum or a product of non-negative doubles
// takes away at most a factor (1 - 2^-53) and 2^-1074, (1 + 2^-40)(1 - 2^-53)^3
// > 1, and with |s| >= 2^-900 the last term is exact and far above those
// 2^-1074. h is half the smaller of the two gaps between s and its neighbours
// (both exact: neighbouring normal doubles differ by a power of two). If
// b < h, every such Y lies strictly within h of s, so it rounds to s, with no
// tie, and X is not zero (|X - s| < h <= |s|). Otherwise the exact path of
// today gives the coordinate. So the filter never changes a bit: it only
// skips the exact computation when its result is certain.

constexpr double kSmallestFiltered = 0x1p-900;
constexpr double kLargestFiltered = 0x1p1000;

// The double nearest to every number within 2^-120 |X| of X = c + T, with T
// in `offset`, when the filter decides it; nothing otherwise.
std::optional<double> nearestSum(double c, const exact::Interval& offset) {
    const double t = offset.centre;
    if (!exact::isKnown(offset) || !(std::fabs(t) <= kLargestFiltered)) {
        return std::nullopt;
    }
    const double s = c + t;
    if (!(std::fabs(s) >= kSmallestFiltered && std::fabs(s) <= kLargestFiltered)) {
        return std::nullopt;
    }
    const double tPart = s - c;
    const double cPart = s - tPart;
    const double error = (c - cPart) + (t - tPart);
    const double bound =
        ((std::fabs(error) + offset.radius) * (1.0 + 0x1p-40)) + (std::fabs(s) * 0x1p-110);
    const double above = std::nextafter(s, std::numeric_limits<double>::infinity()) - s;
    const double below = s - std::nextafter(s, -std::numeric_limits<double>::infinity());
    if (bound < 0.5 * std::min(above, below)) {
        return s;
    }
    return std::nullopt;
}

// The filtered coordinates of the point c + r D / |D| of an Arc, each one or
// nothing.
std::pair<std::optional<double>, std::optional<double>> filteredArcPoint(
    const Arc& arc, const ExactVector& direction) {
    if (!exact::filterEnabled()) {
        return {};
    }
    const exact::Interval dx = exact::intervalOf(direction.x);
    const exact::Interval dy = exact::intervalOf(direction.y);
    // The exact |D|^2 is not negative, as squareRoot() asks.
    const exact::Interval length = exact::squareRoot((dx * dx) + (dy * dy));
    const exact::Interval radius = exact::intervalOf(arc.radius);
    auto result = std::pair{nearestSum(arc.center.x, exact::quotient(radius * dx, length)),
                            nearestSum(arc.center.y, exact::quotient(radius * dy, length))};
    exact::recordFilterAnswer(result.first.has_value() && result.second.has_value());
    return result;
}

// An end of an Arc, in the direction D0 or D1 of its meaning: the doubles of
// endpoints() (curves.cpp, pointInDirection), each coordinate from the filter
// or, when it does not decide, from the same exact computation.
Point2 arcEnd(const Arc& arc, const ExactVector& direction) {
    const auto [x, y] = filteredArcPoint(arc, direction);
    if (x && y) {
        return Point2{*x, *y};
    }
    const Dyadic norm = direction.x * direction.x + direction.y * direction.y;
    const Dyadic radius(arc.radius);
    return Point2{
        x ? *x
          : exact::quadraticToDouble(Dyadic(arc.center.x) * norm, radius * direction.x, norm, norm),
        y ? *y
          : exact::quadraticToDouble(Dyadic(arc.center.y) * norm, radius * direction.y, norm,
                                     norm)};
}

// The point of an Arc in an exact direction, correctly rounded: the doubles
// of nearestPoint(pointInDirection()), from the filter or, when it does not
// decide, from that exact computation. `circle` is the circle of the meaning
// of the Arc.
Point2 arcMiddle(const Arc& arc, const CircleSupport& circle, const ExactVector& direction) {
    const auto [x, y] = filteredArcPoint(arc, direction);
    if (x && y) {
        return Point2{*x, *y};
    }
    const Point2 exact = nearestPoint(pointInDirection(circle, direction));
    return Point2{x.value_or(exact.x), y.value_or(exact.y)};
}

bool wholeCircle(const CurveMeaning& meaning) {
    const auto* arc = std::get_if<ArcMeaning>(&meaning.support);
    return arc != nullptr && (arc->span == ArcSpan::whole || arc->span == ArcSpan::circle);
}

// True when a point lies on a curve of step 1, ends included.
bool onRuleCurve(const RuleCurve& curve, const ExactPoint& point) {
    if (!onCarrier(curve.carrier, point)) {
        return false;
    }
    return curve.whole || takesPart(locate(curve.meaning, point), Extension::none);
}

RuleCurve ruleCurve(std::size_t input, const CurveMeaning& meaning, double magnitude) {
    RuleCurve result;
    result.input = input;
    result.meaning = meaning;
    result.carrier = carrierOf(meaning);
    result.magnitude = magnitude;
    std::visit(
        [&](const auto& data) {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, Segment> || std::is_same_v<T, PolylineSegment>) {
                result.start = data.start;
                result.end = data.end;
            } else if constexpr (std::is_same_v<T, Circle>) {
                result.whole = true;
                result.circle = data;
                result.start = Point2{data.center.x + data.radius, data.center.y};
                result.end = result.start;
            } else {
                assert(false && "an Arc is converted before it is kept");
            }
        },
        meaning.curve);
    // boxOf() of the curve with no moved ends: the circle of a circular one,
    // the doubles of the ends of a straight one.
    result.box = boxOf(result.carrier, result.start, result.end);
    return result;
}

// Step 1: the curves kept, the curves left out, the arcs converted.
void keepCurves(BoundaryRule& rule, std::span<const Curve> curves) {
    for (std::size_t i = 0; i < curves.size(); ++i) {
        const Curve& curve = curves[i];
        auto checked = checkedMeaning(curve, i + 1);
        if (!checked.hasValue()) {
            rule.skipped.push_back({i, checked.diagnosis()});
            continue;
        }
        const CurveMeaning meaning = std::move(checked).value();
        const auto* arc = std::get_if<ArcMeaning>(&meaning.support);
        if (arc != nullptr && !circleFits(arc->circle)) {
            rule.skipped.push_back(
                {i, Diagnosis{DiagnosisCode::outOfDomain, i + 1,
                              "a point of the carrying circle is beyond the coordinate domain"}});
            continue;
        }
        const double magnitude = meaning.magnitude;
        if (const auto* given = std::get_if<Arc>(&curve)) {
            if (arc->span == ArcSpan::whole) {
                RuleCurve whole =
                    ruleCurve(i, meaningOf(Circle{given->center, given->radius}), magnitude);
                whole.againstInput = arc->turn == Turn::clockwise;
                rule.curves.push_back(std::move(whole));
                continue;
            }
            for (const PolylineSegment& piece : convertedArc(*given, *arc)) {
                rule.curves.push_back(ruleCurve(i, meaningOf(piece), magnitude));
            }
            continue;
        }
        rule.curves.push_back(ruleCurve(i, meaning, magnitude));
    }
}

Point2 endOf(const BoundaryRule& rule, std::size_t end) {
    const RuleCurve& curve = rule.curves[end / 2];
    return end % 2 == 0 ? curve.start : curve.end;
}

// Step 1: the free ends, those that are not a common point with another
// curve. Only the pairs of curves whose boxes overlap can share a point.
void findFreeEnds(BoundaryRule& rule) {
    const std::size_t count = rule.curves.size();
    rule.freeEnd.assign(2 * count, false);
    rule.linked.assign(2 * count, false);
    rule.moved.assign(2 * count, std::nullopt);
    std::vector<Box> boxes;
    boxes.reserve(count);
    for (std::size_t k = 0; k < count; ++k) {
        const bool hasEnds = !rule.curves[k].whole;
        rule.freeEnd[2 * k] = hasEnds;
        rule.freeEnd[2 * k + 1] = hasEnds;
        boxes.push_back(rule.curves[k].box);
    }
    const auto clearOn = [&](std::size_t from, std::size_t onto) {
        const RuleCurve& curve = rule.curves[onto];
        for (const std::size_t end : {2 * from, 2 * from + 1}) {
            if (!rule.freeEnd[end]) {
                continue;
            }
            const Point2 point = endOf(rule, end);
            // An end of the data of a curve with ends lies on it: its carrier
            // passes exactly through its ends (curve_meaning.hpp), and there
            // it is atStart or atEnd. So an equal end needs no exact test.
            const bool atAnEnd =
                !curve.whole && (samePoints(point, curve.start) || samePoints(point, curve.end));
            assert(!atAnEnd || onRuleCurve(curve, exactPoint(point)));
            if (atAnEnd || onRuleCurve(curve, exactPoint(point))) {
                rule.freeEnd[end] = false;
            }
        }
    };
    for (const auto& [i, j] : overlappingPairs(boxes)) {
        clearOn(i, j);
        clearOn(j, i);
    }
}

bool curvedPiece(const RuleCurve& curve) {
    return !curve.whole && std::holds_alternative<ArcMeaning>(curve.meaning.support);
}

// Step 2: a link from every free end to every other end within the
// tolerance of their two curves (the other end of the same curve only for a
// curved piece), once for each pair.
void addLinks(BoundaryRule& rule, const robust::TolerancePolicy& policy) {
    std::vector<std::size_t> ends;
    for (std::size_t end = 0; end < rule.freeEnd.size(); ++end) {
        if (!rule.curves[end / 2].whole) {
            ends.push_back(end);
        }
    }
    std::stable_sort(ends.begin(), ends.end(), [&](std::size_t a, std::size_t b) {
        return endOf(rule, a).x < endOf(rule, b).x;
    });
    // Twice the largest tolerance: the window only skips ends that are too
    // far, whatever the rounding of its bounds.
    const double window = 2 * policy.epsilon(rule.largestMagnitude);
    std::set<std::pair<std::size_t, std::size_t>> pairs;
    for (const std::size_t e : ends) {
        if (!rule.freeEnd[e]) {
            continue;
        }
        const Point2 here = endOf(rule, e);
        const RuleCurve& curve = rule.curves[e / 2];
        const auto first =
            std::lower_bound(ends.begin(), ends.end(), here.x - window,
                             [&](std::size_t end, double x) { return endOf(rule, end).x < x; });
        for (auto it = first; it != ends.end() && endOf(rule, *it).x <= here.x + window; ++it) {
            const std::size_t f = *it;
            const Point2 there = endOf(rule, f);
            if (f == e || (f / 2 == e / 2 && !curvedPiece(curve)) || samePoints(here, there)) {
                continue;
            }
            const RuleCurve& other = rule.curves[f / 2];
            const double eps = policy.epsilon(std::max(curve.magnitude, other.magnitude));
            if (within(here, there, eps)) {
                pairs.emplace(std::min(e, f), std::max(e, f));
            }
        }
    }
    for (const auto& [a, b] : pairs) {
        const RuleCurve& first = rule.curves[a / 2];
        const RuleCurve& second = rule.curves[b / 2];
        rule.links.push_back(RuleLink{
            a, b, endOf(rule, a), endOf(rule, b), std::min(first.input, second.input),
            std::max(first.input, second.input), std::max(first.magnitude, second.magnitude)});
        rule.linked[a] = true;
        rule.linked[b] = true;
    }
    std::stable_sort(
        rule.links.begin(), rule.links.end(), [](const RuleLink& a, const RuleLink& b) {
            return std::pair{a.lowCurve, a.highCurve} < std::pair{b.lowCurve, b.highCurve};
        });
}

// Step 3 for one end: the first point on the extension of its curve beyond
// the end common to another curve of step 1, if the tolerance of the curve
// and of a curve met there reaches it.
std::optional<ExactPoint> extension(const BoundaryRule& rule, std::size_t end,
                                    const robust::TolerancePolicy& policy) {
    const std::size_t k = end / 2;
    const bool atStart = end % 2 == 0;
    const RuleCurve& curve = rule.curves[k];
    const Point2 here = endOf(rule, end);
    const bool circular = std::holds_alternative<CircleSupport>(curve.carrier);
    // A straight extension meets a point within the tolerance only inside
    // this box; a circular one may come back near the end only around its
    // circle, whose box is the curve's.
    const double window = 2 * policy.epsilon(rule.largestMagnitude);
    const Box search =
        circular ? curve.box
                 : Box{here.x - window, here.y - window, here.x + window, here.y + window};

    std::optional<ExactPoint> first;
    double firstMagnitude = 0.0;
    const auto consider = [&](const ExactPoint& point, const RuleCurve& other) {
        const Location location = locate(curve.meaning, point);
        const bool beyond = circular || !atStart ? location == Location::afterEnd
                                                 : location == Location::beforeStart;
        if (!beyond) {
            return;
        }
        if (!first) {
            first = point;
            firstMagnitude = other.magnitude;
            return;
        }
        // Forward from the end, backward from the start.
        Sign order = compareAlong(curve.meaning, point, *first);
        if (atStart) {
            order = negated(order);
        }
        if (isNegative(order)) {
            first = point;
            firstMagnitude = other.magnitude;
        } else if (order == Sign::zero) {
            firstMagnitude = std::max(firstMagnitude, other.magnitude);
        }
    };
    for (std::size_t j = 0; j < rule.curves.size(); ++j) {
        const RuleCurve& other = rule.curves[j];
        if (j == k || !overlaps(search, other.box)) {
            continue;
        }
        const CarrierMeeting meeting = meet(curve.carrier, other.carrier);
        if (meeting.sameCarrier) {
            // The end is free, so it is not on the other curve: the first
            // common point is an end of it. A whole circle would hold the end.
            if (!other.whole) {
                consider(exactPoint(other.start), other);
                consider(exactPoint(other.end), other);
            }
            continue;
        }
        for (const CarrierPoint& found : meeting.found()) {
            if (onRuleCurve(other, found.point)) {
                consider(found.point, other);
            }
        }
    }
    if (first && within(here, *first, policy.epsilon(std::max(curve.magnitude, firstMagnitude)))) {
        return first;
    }
    return std::nullopt;
}

}  // namespace

Sign compareFrom(const CurveMeaning& curve, const ExactPoint& origin, const ExactPoint& a,
                 const ExactPoint& b) {
    const bool aWraps = isNegative(compareAlong(curve, a, origin));
    const bool bWraps = isNegative(compareAlong(curve, b, origin));
    if (aWraps != bWraps) {
        return aWraps ? Sign::positive : Sign::negative;
    }
    return compareAlong(curve, a, b);
}

bool onStretch(const ArrangementCurve& curve, const ExactPoint& point) {
    if (wholeCircle(curve.meaning)) {
        return true;
    }
    const ExactPoint start = curve.start.value_or(startPoint(curve.meaning));
    const ExactPoint end = curve.end.value_or(endPoint(curve.meaning));
    if (std::holds_alternative<LineMeaning>(curve.meaning.support)) {
        return !isPositive(compareAlong(curve.meaning, start, point)) &&
               !isPositive(compareAlong(curve.meaning, point, end));
    }
    return !isPositive(compareFrom(curve.meaning, start, point, end));
}

double arcBulge(Point2 from, Point2 to, Point2 centre, double radius, bool counterclockwise) {
    const double halfChord = 0.5 * distance(from, to);
    const Point2 middle{(from.x + to.x) * 0.5, (from.y + to.y) * 0.5};
    const double denominator = radius + distance(middle, centre);
    // Zero only for data far below the bounds of curves.hpp (M < 2^-50).
    if (!(denominator > 0.0)) {
        return 0.0;
    }
    const double bulge = halfChord / denominator;
    return counterclockwise ? bulge : -bulge;
}

Point2 stretchMiddle(const CircleSupport& circle, Point2 from, Point2 to, bool counterclockwise) {
    // For a stretch that turns by theta in (pi, 2 pi), the chord turned a
    // quarter back (forward for a clockwise stretch) and the centre minus the
    // sum of the ends both point from the centre to the middle, with lengths
    // 2 r sin(theta / 2) and -2 r cos(theta / 2): their sum is at least 2 r
    // long, so the rounding of the ends moves its direction by a few units of
    // 2^-53 * max(|p|, |centre|) / r at most, and the point by r times that.
    const double centreX = quotientToDouble(circle.centerX, circle.weight);
    const double centreY = quotientToDouble(circle.centerY, circle.weight);
    const Dyadic chordX = Dyadic(to.x) - Dyadic(from.x);
    const Dyadic chordY = Dyadic(to.y) - Dyadic(from.y);
    const ExactVector normal =
        counterclockwise ? ExactVector{chordY, -chordX} : ExactVector{-chordY, chordX};
    const Dyadic sumX = Dyadic(from.x) + Dyadic(to.x) - Dyadic(centreX).timesPowerOfTwo(1);
    const Dyadic sumY = Dyadic(from.y) + Dyadic(to.y) - Dyadic(centreY).timesPowerOfTwo(1);
    ExactVector direction{normal.x - sumX, normal.y - sumY};
    // Only far below the bounds of curves.hpp (M < 2^-50).
    if (direction.x.isZero() && direction.y.isZero()) {
        direction = ExactVector{Dyadic(1.0), Dyadic()};
    }
    return nearestPoint(pointInDirection(circle, direction));
}

std::vector<PolylineSegment> convertedArc(const Arc& arc, const ArcMeaning& meaning) {
    // The rounded ends of endpoints() (curves.hpp), from the directions D0
    // and D1 of the meaning: they are the ones endpoints() computes, since
    // the meaning of a checked Arc is meaningOf() of it.
    assert(meaning.span == ArcSpan::partial && "a checked arc that is not a whole circle");
    const Point2 start = arcEnd(arc, meaning.startDirection);
    const Point2 end = arcEnd(arc, meaning.endDirection);
    const bool counterclockwise = meaning.turn == Turn::counterclockwise;
    // An Arc whose rounded ends coincide is dropped (boundary.hpp, step 1).
    if (samePoints(start, end)) {
        return {};
    }
    std::vector<Point2> corners{start};
    if (meaning.endSector > TurnSector::half) {
        // Above half a turn: split at the middle, in the direction from the
        // centre given by the directions D0 and D1 of the ends, exactly as
        // stretchMiddle() does with the ends (D0 and D1 are within a few
        // units of 2^-53 of unit vectors).
        const ExactVector& d0 = meaning.startDirection;
        const ExactVector& d1 = meaning.endDirection;
        const Dyadic chordX = d1.x - d0.x;
        const Dyadic chordY = d1.y - d0.y;
        const ExactVector normal =
            counterclockwise ? ExactVector{chordY, -chordX} : ExactVector{-chordY, chordX};
        const ExactVector direction{normal.x - d0.x - d1.x, normal.y - d0.y - d1.y};
        corners.push_back(arcMiddle(arc, meaning.circle, direction));
    }
    corners.push_back(end);

    std::vector<PolylineSegment> pieces;
    for (std::size_t k = 0; k + 1 < corners.size(); ++k) {
        const Point2 from = corners[k];
        const Point2 to = corners[k + 1];
        if (samePoints(from, to)) {
            continue;
        }
        pieces.push_back(PolylineSegment{
            from, to, arcBulge(from, to, arc.center, arc.radius, counterclockwise)});
    }
    return pieces;
}

BoundaryRule applyRule(std::span<const Curve> curves, const robust::TolerancePolicy& policy) {
    BoundaryRule rule;
    keepCurves(rule, curves);
    for (const RuleCurve& curve : rule.curves) {
        rule.largestMagnitude = std::max(rule.largestMagnitude, curve.magnitude);
    }
    findFreeEnds(rule);
    addLinks(rule, policy);
    for (std::size_t end = 0; end < rule.freeEnd.size(); ++end) {
        if (rule.freeEnd[end] && !rule.linked[end]) {
            rule.moved[end] = extension(rule, end, policy);
        }
    }
    return rule;
}

std::vector<ArrangementCurve> arrangementCurves(const BoundaryRule& rule, std::size_t inputCount) {
    std::vector<ArrangementCurve> result;
    result.reserve(rule.curves.size() + rule.links.size());
    for (std::size_t k = 0; k < rule.curves.size(); ++k) {
        const RuleCurve& curve = rule.curves[k];
        ArrangementCurve given{curve.meaning, std::nullopt, std::nullopt, curve.input};
        if (!curve.whole) {
            const auto& start = rule.moved[2 * k];
            const auto& end = rule.moved[2 * k + 1];
            if (start && end && samePoint(*start, *end)) {
                // Both ends of a curved piece reach the only point of its
                // extension common to another curve: the piece covers its
                // whole circle, and that point is on it.
                auto& arc = std::get<ArcMeaning>(given.meaning.support);
                arc.span = ArcSpan::whole;
                arc.endDirection = arc.startDirection;
                arc.endSector = TurnSector::zero;
            } else {
                given.start = start;
                given.end = end;
            }
        }
        result.push_back(std::move(given));
    }
    for (std::size_t l = 0; l < rule.links.size(); ++l) {
        const RuleLink& link = rule.links[l];
        result.push_back(ArrangementCurve{meaningOf(Segment{link.from, link.to}), std::nullopt,
                                          std::nullopt, inputCount + l});
    }
    return result;
}

namespace {

// Step 4 for one curve or link: whether the exact distance from the picked
// point to its stretch is at most eps.
bool withinStretch(const ArrangementCurve& curve, Point2 picked, double eps) {
    const Carrier carrier = carrierOf(curve.meaning);
    if (const auto* line = std::get_if<LineMeaning>(&carrier)) {
        const ExactPoint start = curve.start.value_or(startPoint(curve.meaning));
        const ExactPoint end = curve.end.value_or(endPoint(curve.meaning));
        const ExactVector direction{Dyadic(line->end.x) - Dyadic(line->start.x),
                                    Dyadic(line->end.y) - Dyadic(line->start.y)};
        if (isNegative(alongSign(direction, start, picked))) {
            return within(picked, start, eps);
        }
        if (isPositive(alongSign(direction, end, picked))) {
            return within(picked, end, eps);
        }
        // The foot of the perpendicular is on the stretch: the distance is
        // |cross(direction, q - start)| / |direction|.
        const Dyadic cross = direction.x * (Dyadic(picked.y) - Dyadic(line->start.y)) -
                             direction.y * (Dyadic(picked.x) - Dyadic(line->start.x));
        return compare(square(cross),
                       square(Dyadic(eps)) * (square(direction.x) + square(direction.y))) !=
               Sign::positive;
    }
    // A circle: centre c / w, radius sqrt(rho) / w. With v = w q - c, the
    // distance to the circle is |sqrt(|v|^2) - sqrt(rho)| / w.
    const auto& circle = std::get<CircleSupport>(carrier);
    const Dyadic& w = circle.weight;
    const ExactVector v{w * Dyadic(picked.x) - circle.centerX,
                        w * Dyadic(picked.y) - circle.centerY};
    const Dyadic& rho = circle.radiusSquared;
    const Dyadic scaled = Dyadic(eps) * w;
    const Dyadic scaledSquare = square(scaled);
    if (v.x.isZero() && v.y.isZero()) {
        // The centre: every point of the circle is at the radius.
        return compare(rho, scaledSquare) != Sign::positive;
    }
    bool nearestOnStretch = wholeCircle(curve.meaning);
    ExactPoint start;
    ExactPoint end;
    if (!nearestOnStretch) {
        start = curve.start.value_or(startPoint(curve.meaning));
        end = curve.end.value_or(endPoint(curve.meaning));
        nearestOnStretch =
            !isPositive(compareFrom(curve.meaning, start, pointInDirection(circle, v), end));
    }
    if (!nearestOnStretch) {
        // The nearest point of the stretch is one of its ends.
        return within(picked, start, eps) || within(picked, end, eps);
    }
    const Dyadic norm = square(v.x) + square(v.y);
    const Dyadic twice = scaled.timesPowerOfTwo(1);
    // sqrt(norm) <= sqrt(rho) + e: norm <= rho + 2 e sqrt(rho) + e^2.
    if (isNegative(exact::signOf(rho + scaledSquare - norm, twice, rho))) {
        return false;
    }
    // sqrt(norm) >= sqrt(rho) - e, when sqrt(rho) > e.
    if (compare(rho, scaledSquare) != Sign::positive) {
        return true;
    }
    return !isNegative(exact::signOf(norm - rho - scaledSquare, twice, rho));
}

}  // namespace

std::vector<Box> arrangementBoxes(const BoundaryRule& rule,
                                  std::span<const ArrangementCurve> curves) {
    std::vector<Box> boxes;
    boxes.reserve(curves.size());
    for (std::size_t a = 0; a < curves.size(); ++a) {
        // A curve of step 1 keeps the box of step 1 unless an end of a
        // straight one was moved: a circular one has the box of its circle.
        if (a < rule.curves.size()) {
            const RuleCurve& curve = rule.curves[a];
            const bool moved = !curve.whole &&
                               (rule.moved[2 * a].has_value() || rule.moved[2 * a + 1].has_value());
            if (!moved || std::holds_alternative<CircleSupport>(curve.carrier)) {
                boxes.push_back(curve.box);
                continue;
            }
        }
        boxes.push_back(boxOf(curves[a]));
    }
    return boxes;
}

std::optional<std::size_t> nearestCurve(const BoundaryRule& rule,
                                        std::span<const ArrangementCurve> curves,
                                        std::span<const Box> boxes, Point2 picked,
                                        const robust::TolerancePolicy& policy) {
    const double scale = std::max(std::fabs(picked.x), std::fabs(picked.y));
    const double window = 2 * policy.epsilon(std::max(rule.largestMagnitude, scale));
    const Box near{picked.x - window, picked.y - window, picked.x + window, picked.y + window};
    std::optional<std::size_t> nearest;
    for (std::size_t a = 0; a < curves.size(); ++a) {
        const bool isCurve = a < rule.curves.size();
        const std::size_t index =
            isCurve ? rule.curves[a].input : rule.links[a - rule.curves.size()].lowCurve;
        if ((nearest && *nearest <= index) || !overlaps(boxes[a], near)) {
            continue;
        }
        const double magnitude =
            isCurve ? rule.curves[a].magnitude : rule.links[a - rule.curves.size()].magnitude;
        if (withinStretch(curves[a], picked, policy.epsilon(std::max(magnitude, scale)))) {
            nearest = index;
        }
    }
    return nearest;
}

}  // namespace solid_space::geom2d::detail

namespace solid_space::geom2d {
namespace {

using detail::Arrangement;
using detail::ArrangementCurve;
using detail::BoundaryRule;
using detail::Box;
using detail::CircleSupport;
using detail::Dyadic;
using detail::ExactPoint;
using detail::Sign;
using exact::Tower;
using exact::TowerNumber;

constexpr std::size_t kNone = std::numeric_limits<std::size_t>::max();
constexpr double kInfinity = std::numeric_limits<double>::infinity();

bool isNegative(Sign sign) noexcept {
    return sign == Sign::negative;
}

bool isPositive(Sign sign) noexcept {
    return sign == Sign::positive;
}

// --- Picked point ------------------------------------------------------------

std::optional<Diagnosis> checkPicked(Point2 picked) {
    if (!std::isfinite(picked.x) || !std::isfinite(picked.y)) {
        return Diagnosis{DiagnosisCode::invalidValue, 0, "the picked point is not finite"};
    }
    if (std::fabs(picked.x) > robust::kMaxCoordinate ||
        std::fabs(picked.y) > robust::kMaxCoordinate) {
        return Diagnosis{DiagnosisCode::outOfDomain, 0,
                         "the picked point is beyond the coordinate domain"};
    }
    return std::nullopt;
}

// --- Boxes of loops ------------------------------------------------------------

// As arrangement.cpp: far wider than any rounding, so that a box always
// contains what it stands for. Boxes only skip pairs; no decision reads them.
constexpr double kBoxRelativeMargin = 0x1p-40;
constexpr double kBoxAbsoluteMargin = 0x1p-1000;

void include(Box& box, Point2 point) {
    const double mx = std::fabs(point.x) * kBoxRelativeMargin + kBoxAbsoluteMargin;
    const double my = std::fabs(point.y) * kBoxRelativeMargin + kBoxAbsoluteMargin;
    box.minX = std::min(box.minX, point.x - mx);
    box.maxX = std::max(box.maxX, point.x + mx);
    box.minY = std::min(box.minY, point.y - my);
    box.maxY = std::max(box.maxY, point.y + my);
}

void include(Box& box, const Box& other) {
    box.minX = std::min(box.minX, other.minX);
    box.minY = std::min(box.minY, other.minY);
    box.maxX = std::max(box.maxX, other.maxX);
    box.maxY = std::max(box.maxY, other.maxY);
}

bool contains(const Box& box, Point2 point) {
    return box.minX <= point.x && point.x <= box.maxX && box.minY <= point.y && point.y <= box.maxY;
}

// --- Vertices of the returned loops ------------------------------------------

// A vertex of a returned loop: a vertex of the arrangement, the single point
// of a Circle for the order of the loops, or the middle of a stretch split in
// two (from `point` to `to` on `circle`, turning by more than half a turn).
struct LoopVertex {
    ExactPoint point;
    std::size_t vertex = kNone;
    bool middle = false;
    CircleSupport circle;
    ExactPoint to;
};

// A point of a tower: (x / weight, y / weight), with a positive weight.
struct TowerPoint {
    TowerNumber x;
    TowerNumber y;
    TowerNumber weight;
};

TowerNumber rootOf(Tower& tower, const TowerNumber& radicand) {
    // At most three roots for each of the two points compared: within
    // exact::kMaxTowerRoots, so a root is always available.
    const auto root = tower.squareRoot(radicand);
    assert(root.has_value() && "two vertices need at most six roots");
    return root.value_or(TowerNumber());
}

// The numerators (a + b sqrt(d)) of the coordinates of an exact point.
std::pair<TowerNumber, TowerNumber> numerators(Tower& tower, const ExactPoint& point) {
    const TowerNumber root = rootOf(tower, TowerNumber(point.radicand));
    return {TowerNumber(point.rational.x) + root * point.radical.x,
            TowerNumber(point.rational.y) + root * point.radical.y};
}

// The middle of the stretch from F to T of a circle with centre c / w and
// squared radius rho / w^2, turning by more than half a turn: opposite to
// S = F + T - 2 C from the centre, C - r S / |S|. With S = S' / D, D the
// product of the weights, and N = |S'|^2:
//     C - r S / |S| = (c N - S' sqrt(rho N)) / (w N).
TowerPoint inTower(Tower& tower, const LoopVertex& vertex) {
    const auto [px, py] = numerators(tower, vertex.point);
    if (!vertex.middle) {
        return {px, py, TowerNumber(vertex.point.weight)};
    }
    const auto [qx, qy] = numerators(tower, vertex.to);
    const CircleSupport& circle = vertex.circle;
    const Dyadic& wf = vertex.point.weight;
    const Dyadic& wt = vertex.to.weight;
    const Dyadic& w = circle.weight;
    const Dyadic both = (wf * wt).timesPowerOfTwo(1);
    const TowerNumber sx = px * (wt * w) + qx * (wf * w) - TowerNumber(circle.centerX * both);
    const TowerNumber sy = py * (wt * w) + qy * (wf * w) - TowerNumber(circle.centerY * both);
    const TowerNumber norm = tower.product(sx, sx) + tower.product(sy, sy);
    const TowerNumber root = rootOf(tower, norm * circle.radiusSquared);
    return {circle.centerX * norm - tower.product(sx, root),
            circle.centerY * norm - tower.product(sy, root), norm * w};
}

// The exact lexicographic order of (x, y): the sign of a - b.
Sign compareVertices(const LoopVertex& a, const LoopVertex& b) {
    if (!a.middle && !b.middle) {
        if (a.vertex != kNone && b.vertex != kNone) {
            // The vertices of the arrangement are numbered in this order.
            if (a.vertex == b.vertex) {
                return Sign::zero;
            }
            return a.vertex < b.vertex ? Sign::negative : Sign::positive;
        }
        const Sign x = detail::compareX(a.point, b.point);
        return x != Sign::zero ? x : detail::compareY(a.point, b.point);
    }
    Tower tower;
    const TowerPoint pa = inTower(tower, a);
    const TowerPoint pb = inTower(tower, b);
    const Sign x = tower.sign(tower.product(pa.x, pb.weight) - tower.product(pb.x, pa.weight));
    if (x != Sign::zero) {
        return x;
    }
    return tower.sign(tower.product(pa.y, pb.weight) - tower.product(pb.y, pa.weight));
}

// The antipode 2 C - F of an exact point F of a circle: the same radicand.
ExactPoint antipode(const CircleSupport& circle, const ExactPoint& point) {
    ExactPoint result;
    const Dyadic& w = circle.weight;
    const Dyadic twice = point.weight.timesPowerOfTwo(1);
    result.rational = detail::ExactVector{circle.centerX * twice - w * point.rational.x,
                                          circle.centerY * twice - w * point.rational.y};
    result.radical = detail::ExactVector{-(w * point.radical.x), -(w * point.radical.y)};
    result.radicand = point.radicand;
    result.weight = w * point.weight;
    return result;
}

// --- Building the loops --------------------------------------------------------

// A loop being returned, with what orders it among the others.
struct BuiltLoop {
    RegionLoop region;
    std::vector<LoopVertex> sequence;
    std::vector<std::size_t> darts;
    Box box;
};

class LoopBuilder {
public:
    LoopBuilder(const Arrangement& arrangement, const BoundaryRule& rule,
                std::span<const ArrangementCurve> curves)
        : m_arrangement(arrangement),
          m_rule(rule),
          m_curves(curves),
          m_rounded(arrangement.vertexCount()) {
        // The ends of the data of step 1, by their doubles, for the vertices
        // returned with the doubles of the data (+0 and -0 are one key).
        for (const detail::RuleCurve& curve : rule.curves) {
            if (!curve.whole) {
                m_dataEnds.emplace(key(curve.start), curve.start);
                m_dataEnds.emplace(key(curve.end), curve.end);
            }
        }
    }

    // A simple loop of darts that runs counterclockwise.
    [[nodiscard]] BuiltLoop build(std::vector<std::size_t> darts) const;

private:
    static std::pair<double, double> key(Point2 point) {
        // Adding +0 turns -0 into +0 and changes nothing else.
        return {point.x + 0.0, point.y + 0.0};
    }

    [[nodiscard]] Point2 rounded(std::size_t vertex) const;
    [[nodiscard]] LoopPieceOrigin originOf(std::size_t dart) const;
    [[nodiscard]] Box boxOf(std::span<const std::size_t> darts) const;

    const Arrangement& m_arrangement;
    const BoundaryRule& m_rule;
    std::span<const ArrangementCurve> m_curves;
    std::map<std::pair<double, double>, Point2> m_dataEnds;
    mutable std::vector<std::optional<Point2>> m_rounded;
};

Point2 LoopBuilder::rounded(std::size_t vertex) const {
    auto& cached = m_rounded[vertex];
    if (!cached) {
        const ExactPoint& exactVertex = m_arrangement.vertex(vertex);
        Point2 point = m_arrangement.roundedVertex(vertex);
        // A vertex that is an end of the data is a double: nearestPoint()
        // gives it exactly, and the data give the sign of its zeros.
        if (const auto found = m_dataEnds.find(key(point));
            found != m_dataEnds.end() &&
            detail::samePoint(exactVertex, detail::exactPoint(found->second))) {
            point = found->second;
        }
        cached = point;
    }
    return *cached;
}

LoopPieceOrigin LoopBuilder::originOf(std::size_t dart) const {
    const std::size_t curve = m_arrangement.pieces()[Arrangement::pieceOf(dart)].curve;
    if (curve < m_rule.curves.size()) {
        const detail::RuleCurve& given = m_rule.curves[curve];
        return LoopPieceOrigin{LoopPieceSource::curve, given.input, given.input,
                               Arrangement::reversed(dart) != given.againstInput};
    }
    const detail::RuleLink& link = m_rule.links[curve - m_rule.curves.size()];
    return LoopPieceOrigin{LoopPieceSource::link, link.lowCurve, link.highCurve, false};
}

Box LoopBuilder::boxOf(std::span<const std::size_t> darts) const {
    Box box{kInfinity, kInfinity, -kInfinity, -kInfinity};
    for (const std::size_t dart : darts) {
        const std::size_t curve = m_arrangement.pieces()[Arrangement::pieceOf(dart)].curve;
        if (std::holds_alternative<detail::ArcMeaning>(m_curves[curve].meaning.support)) {
            include(box, detail::boxOf(m_curves[curve]));
        } else {
            include(box, m_arrangement.roundedVertex(m_arrangement.tail(dart)));
            include(box, m_arrangement.roundedVertex(m_arrangement.head(dart)));
        }
    }
    return box;
}

BuiltLoop LoopBuilder::build(std::vector<std::size_t> darts) const {
    BuiltLoop built;
    built.box = boxOf(darts);
    const auto curveOf = [&](std::size_t dart) {
        return m_arrangement.pieces()[Arrangement::pieceOf(dart)].curve;
    };

    // A whole input circle, credited with every piece of the loop.
    const std::size_t first = curveOf(darts.front());
    const bool single = std::all_of(darts.begin(), darts.end(),
                                    [&](std::size_t dart) { return curveOf(dart) == first; });
    if (single && first < m_rule.curves.size() && m_rule.curves[first].whole) {
        const detail::RuleCurve& given = m_rule.curves[first];
        built.region.loop = given.circle;
        built.region.origins.push_back(originOf(darts.front()));
        LoopVertex vertex;
        vertex.point.rational =
            detail::ExactVector{Dyadic(given.circle.center.x) - Dyadic(given.circle.radius),
                                Dyadic(given.circle.center.y)};
        built.sequence.push_back(vertex);
        built.darts = std::move(darts);
        return built;
    }

    // The segments, from the tail of each dart; a curved stretch above half
    // a turn as two segments of equal angle.
    std::vector<PolylineVertex> vertices;
    std::vector<LoopPieceOrigin> origins;
    std::vector<LoopVertex> sequence;
    for (const std::size_t dart : darts) {
        const std::size_t tail = m_arrangement.tail(dart);
        const std::size_t head = m_arrangement.head(dart);
        const LoopPieceOrigin origin = originOf(dart);
        LoopVertex start;
        start.point = m_arrangement.vertex(tail);
        start.vertex = tail;
        const ArrangementCurve& curve = m_curves[curveOf(dart)];
        const auto* arc = std::get_if<detail::ArcMeaning>(&curve.meaning.support);
        if (arc == nullptr) {
            vertices.push_back({rounded(tail), 0.0});
            origins.push_back(origin);
            sequence.push_back(std::move(start));
            continue;
        }
        const CircleSupport& circle = arc->circle;
        const bool counterclockwise =
            (arc->turn == detail::Turn::counterclockwise) != Arrangement::reversed(dart);
        const Point2 centre{quotientToDouble(circle.centerX, circle.weight),
                            quotientToDouble(circle.centerY, circle.weight)};
        const double radius =
            std::sqrt(quotientToDouble(circle.radiusSquared, circle.weight * circle.weight));
        const Point2 from = rounded(tail);
        const Point2 to = rounded(head);
        // Above half a turn when the antipode of the start of the piece lies
        // strictly inside it (a piece from a vertex back to it is a whole turn).
        const detail::ArrangementPiece& piece = m_arrangement.pieces()[Arrangement::pieceOf(dart)];
        const ExactPoint& pieceFrom = m_arrangement.vertex(piece.from);
        const bool split =
            piece.from == piece.to ||
            isNegative(detail::compareFrom(curve.meaning, pieceFrom, antipode(circle, pieceFrom),
                                           m_arrangement.vertex(piece.to)));
        if (!split) {
            vertices.push_back(
                {from, detail::arcBulge(from, to, centre, radius, counterclockwise)});
            origins.push_back(origin);
            sequence.push_back(std::move(start));
            continue;
        }
        const Point2 middle = detail::stretchMiddle(circle, from, to, counterclockwise);
        vertices.push_back(
            {from, detail::arcBulge(from, middle, centre, radius, counterclockwise)});
        vertices.push_back(
            {middle, detail::arcBulge(middle, to, centre, radius, counterclockwise)});
        origins.push_back(origin);
        origins.push_back(origin);
        LoopVertex halfway;
        halfway.point = start.point;
        halfway.middle = true;
        halfway.circle = circle;
        halfway.to = m_arrangement.vertex(head);
        sequence.push_back(std::move(start));
        sequence.push_back(std::move(halfway));
    }

    // Start at the exactly smallest vertex.
    std::size_t smallest = 0;
    for (std::size_t k = 1; k < sequence.size(); ++k) {
        if (isNegative(compareVertices(sequence[k], sequence[smallest]))) {
            smallest = k;
        }
    }
    const auto offset = static_cast<std::ptrdiff_t>(smallest);
    std::rotate(vertices.begin(), vertices.begin() + offset, vertices.end());
    std::rotate(origins.begin(), origins.begin() + offset, origins.end());
    std::rotate(sequence.begin(), sequence.begin() + offset, sequence.end());
    built.region.loop = Polyline{std::move(vertices), true};
    built.region.origins = std::move(origins);
    built.sequence = std::move(sequence);
    built.darts = std::move(darts);
    return built;
}

// The darts of a simple loop, run counterclockwise.
std::vector<std::size_t> counterclockwiseDarts(const Arrangement& arrangement,
                                               std::vector<std::size_t> loop) {
    if (!arrangement.counterclockwise(loop)) {
        std::reverse(loop.begin(), loop.end());
        for (std::size_t& dart : loop) {
            dart = Arrangement::twin(dart);
        }
    }
    return loop;
}

// The order of the islands at equal depth: their vertex sequences, a
// shorter one first when it is the start of the other.
bool sequenceBefore(const BuiltLoop& a, const BuiltLoop& b) {
    const std::size_t count = std::min(a.sequence.size(), b.sequence.size());
    for (std::size_t k = 0; k < count; ++k) {
        const Sign order = compareVertices(a.sequence[k], b.sequence[k]);
        if (order != Sign::zero) {
            return isNegative(order);
        }
    }
    if (a.sequence.size() != b.sequence.size()) {
        return a.sequence.size() < b.sequence.size();
    }
    // Two loops with the same vertices in the same order: the order of their
    // first darts, so that the result stays deterministic.
    return a.darts.front() < b.darts.front();
}

// --- Steps 5-7 -------------------------------------------------------------------

// Step 5 with an unbounded face: the free ends of the first connected group
// met by the half-line from the picked point toward +x that has free ends.
std::vector<OpenEnd> openEnds(const Arrangement& arrangement, const BoundaryRule& rule,
                              std::span<const ArrangementCurve> curves, std::span<const Box> boxes,
                              Point2 picked) {
    const ExactPoint origin = detail::exactPoint(picked);
    // The line y = picked.y, directed toward +x.
    const double ahead = picked.x + std::max(1.0, std::fabs(picked.x));
    const detail::Carrier ray = detail::LineMeaning{picked, Point2{ahead, picked.y}};
    const Box rayBox{picked.x, picked.y, kInfinity, picked.y};

    // The first point of each group on the half-line.
    std::vector<std::optional<ExactPoint>> firstMeeting(arrangement.groupCount());
    const auto consider = [&](std::size_t group, const ExactPoint& point) {
        if (!isPositive(detail::compareX(point, origin))) {
            return;
        }
        auto& best = firstMeeting[group];
        if (!best || isNegative(detail::compareX(point, *best))) {
            best = point;
        }
    };
    for (std::size_t a = 0; a < curves.size(); ++a) {
        const ArrangementCurve& curve = curves[a];
        if (!detail::overlaps(boxes[a], rayBox)) {
            continue;
        }
        const std::size_t group = arrangement.groupOfCurve(a);
        const detail::CarrierMeeting meeting = detail::meet(detail::carrierOf(curve.meaning), ray);
        if (meeting.sameCarrier) {
            // A straight stretch along the line: its first point is an end.
            consider(group, curve.start.value_or(detail::startPoint(curve.meaning)));
            consider(group, curve.end.value_or(detail::endPoint(curve.meaning)));
            continue;
        }
        for (const detail::CarrierPoint& found : meeting.found()) {
            if (detail::onStretch(curve, found.point)) {
                consider(group, found.point);
            }
        }
    }

    // The free ends left by steps 2 and 3, by group.
    std::vector<std::vector<OpenEnd>> endsOfGroup(arrangement.groupCount());
    for (std::size_t end = 0; end < rule.freeEnd.size(); ++end) {
        if (rule.leftFree(end)) {
            const detail::RuleCurve& curve = rule.curves[end / 2];
            const bool atStart = end % 2 == 0;
            endsOfGroup[arrangement.groupOfCurve(end / 2)].push_back(
                OpenEnd{curve.input, atStart, atStart ? curve.start : curve.end});
        }
    }

    std::vector<std::size_t> met;
    for (std::size_t g = 0; g < arrangement.groupCount(); ++g) {
        if (firstMeeting[g]) {
            met.push_back(g);
        }
    }
    // Two groups share no point: their first meetings differ.
    std::sort(met.begin(), met.end(), [&](std::size_t a, std::size_t b) {
        return isNegative(detail::compareX(*firstMeeting[a], *firstMeeting[b]));
    });
    for (const std::size_t g : met) {
        if (!endsOfGroup[g].empty()) {
            std::vector<OpenEnd> ends = std::move(endsOfGroup[g]);
            std::sort(ends.begin(), ends.end(), [](const OpenEnd& a, const OpenEnd& b) {
                return std::pair{a.curve, !a.atStart} < std::pair{b.curve, !b.atStart};
            });
            return ends;
        }
    }
    return {};
}

// Steps 6 and 7 with the bounded face whose outer cycle is `face`: the
// contour, the islands and their depths, in the returned order.
std::vector<RegionLoop> enclosedLoops(const Arrangement& arrangement, const BoundaryRule& rule,
                                      std::span<const ArrangementCurve> curves, std::size_t face) {
    const LoopBuilder builder(arrangement, rule, curves);
    std::vector<BuiltLoop> loops;
    std::vector<BuiltLoop> islands;
    for (auto& loop : arrangement.simpleLoops(face)) {
        if (loops.empty() && arrangement.counterclockwise(loop)) {
            // The loop of the face that encloses it: the only one that runs
            // counterclockwise with the face on its left.
            loops.push_back(builder.build(std::move(loop)));
        } else {
            islands.push_back(builder.build(counterclockwiseDarts(arrangement, std::move(loop))));
        }
    }
    assert(loops.size() == 1 && "a bounded face has one outer loop");
    const BuiltLoop& contour = loops.front();

    // The outer boundary of every other group inside the contour.
    const std::size_t contourGroup = arrangement.groupOfCycle(face);
    for (std::size_t g = 0; g < arrangement.groupCount(); ++g) {
        if (g == contourGroup) {
            continue;
        }
        const std::size_t groupVertex = arrangement.groupVertex(g);
        const ExactPoint& vertex = arrangement.vertex(groupVertex);
        if (!contains(contour.box, arrangement.roundedVertex(groupVertex)) ||
            !arrangement.inside(vertex, contour.darts)) {
            continue;
        }
        for (auto& loop : arrangement.simpleLoops(arrangement.holeCycle(g))) {
            islands.push_back(builder.build(counterclockwiseDarts(arrangement, std::move(loop))));
        }
    }
    for (auto& island : islands) {
        loops.push_back(std::move(island));
    }

    // Step 7: the depth of a loop is the number of the others that enclose
    // it. A loop inside another lies in its box.
    for (std::size_t inner = 0; inner < loops.size(); ++inner) {
        std::size_t depth = 0;
        for (std::size_t outer = 0; outer < loops.size(); ++outer) {
            if (outer != inner && detail::overlaps(loops[outer].box, loops[inner].box) &&
                arrangement.encloses(loops[outer].darts, loops[inner].darts)) {
                ++depth;
            }
        }
        loops[inner].region.depth = depth;
    }
    assert(loops.front().region.depth == 0 && "nothing encloses the contour");
    std::stable_sort(loops.begin() + 1, loops.end(), [](const BuiltLoop& a, const BuiltLoop& b) {
        if (a.region.depth != b.region.depth) {
            return a.region.depth < b.region.depth;
        }
        return sequenceBefore(a, b);
    });

    std::vector<RegionLoop> result;
    result.reserve(loops.size());
    for (auto& loop : loops) {
        result.push_back(std::move(loop.region));
    }
    return result;
}

}  // namespace

Result<DetectedBoundary> detectBoundary(Point2 picked, std::span<const Curve> curves,
                                        const robust::TolerancePolicy& policy) {
    if (auto problem = checkPicked(picked)) {
        return std::move(*problem);
    }
    // Steps 1-3.
    const BoundaryRule rule = detail::applyRule(curves, policy);
    const std::vector<ArrangementCurve> given = detail::arrangementCurves(rule, curves.size());
    const std::vector<Box> boxes = detail::arrangementBoxes(rule, given);

    DetectedBoundary result;
    result.skipped = rule.skipped;
    // Step 4.
    if (const auto nearest = detail::nearestCurve(rule, given, boxes, picked, policy)) {
        result.outcome = BoundaryOutcome::onCurve;
        result.nearCurve = *nearest;
        return result;
    }
    if (given.empty()) {
        result.outcome = BoundaryOutcome::notEnclosed;
        return result;
    }
    // Step 5: the point lies on no piece, after step 4.
    const Arrangement arrangement(given, boxes);
    const std::optional<std::size_t> face = arrangement.faceOf(detail::exactPoint(picked));
    if (!face) {
        result.outcome = BoundaryOutcome::notEnclosed;
        result.openEnds = openEnds(arrangement, rule, given, boxes, picked);
        return result;
    }
    // Steps 6 and 7.
    result.outcome = BoundaryOutcome::enclosed;
    result.loops = enclosedLoops(arrangement, rule, given, *face);
    return result;
}

}  // namespace solid_space::geom2d
