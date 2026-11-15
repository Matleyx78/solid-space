// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// The rule of detectBoundary (boundary_rule.hpp; stage 0.3, package PS9).

#include "boundary_rule.hpp"

#include "exact/quadratic.hpp"
#include "positions.hpp"
#include "solid_space/geom2d/intersection.hpp"
#include "solid_space/robust/point.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <optional>
#include <set>
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
    result.box = boxOf(ArrangementCurve{meaning, std::nullopt, std::nullopt, 0});
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
        for (const std::size_t end : {2 * from, 2 * from + 1}) {
            if (rule.freeEnd[end] && onRuleCurve(rule.curves[onto], exactPoint(endOf(rule, end)))) {
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
    const auto ends = endpoints(Curve{arc});
    assert(ends.hasValue() && "a checked arc has ends");
    const bool counterclockwise = meaning.turn == Turn::counterclockwise;
    std::vector<Point2> corners{ends.value().start};
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
        corners.push_back(nearestPoint(pointInDirection(meaning.circle, direction)));
    }
    corners.push_back(ends.value().end);

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

std::optional<std::size_t> nearestCurve(const BoundaryRule& rule,
                                        std::span<const ArrangementCurve> curves, Point2 picked,
                                        const robust::TolerancePolicy& policy) {
    const double scale = std::max(std::fabs(picked.x), std::fabs(picked.y));
    const double window = 2 * policy.epsilon(std::max(rule.largestMagnitude, scale));
    const Box near{picked.x - window, picked.y - window, picked.x + window, picked.y + window};
    std::optional<std::size_t> nearest;
    for (std::size_t a = 0; a < curves.size(); ++a) {
        const bool isCurve = a < rule.curves.size();
        const std::size_t index =
            isCurve ? rule.curves[a].input : rule.links[a - rule.curves.size()].lowCurve;
        if ((nearest && *nearest <= index) || !overlaps(boxOf(curves[a]), near)) {
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
