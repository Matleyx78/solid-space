// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// The exact meetings of the carriers of two curves (carriers.hpp; stage 0.2,
// package P9).
//
// Every formula below works in homogeneous coordinates, so that no division
// is ever taken and every sign is the sign of an exact polynomial in the
// doubles of the curves. The derivations are written next to each formula.

#include "carriers.hpp"

#include "exact/quadratic.hpp"
#include "solid_space/robust/point.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <utility>
#include <variant>

namespace solid_space::geom2d::detail {
namespace {

using exact::QuadraticNumber;

Dyadic squared(const Dyadic& value) {
    return value * value;
}

Dyadic dot(const ExactVector& a, const ExactVector& b) {
    return a.x * b.x + a.y * b.y;
}

Dyadic cross(const ExactVector& a, const ExactVector& b) {
    return a.x * b.y - a.y * b.x;
}

ExactVector plus(const ExactVector& a, const ExactVector& b) {
    return {a.x + b.x, a.y + b.y};
}

ExactVector minus(const ExactVector& a, const ExactVector& b) {
    return {a.x - b.x, a.y - b.y};
}

ExactVector times(const ExactVector& vector, const Dyadic& factor) {
    return {vector.x * factor, vector.y * factor};
}

ExactVector negated(const ExactVector& vector) {
    return {-vector.x, -vector.y};
}

Sign negated(Sign sign) noexcept {
    switch (sign) {
        case Sign::negative:
            return Sign::positive;
        case Sign::positive:
            return Sign::negative;
        case Sign::zero:
            break;
    }
    return Sign::zero;
}

ExactVector centerOf(const CircleSupport& circle) {
    return {circle.centerX, circle.centerY};
}

// The direction of a line, end - start, exactly.
ExactVector directionOf(const LineMeaning& line) {
    return minus(exactVector(line.end), exactVector(line.start));
}

// The sign of (ra + qa sqrt(da)) / wa - (rb + qb sqrt(db)) / wb. Both weights
// are positive, so it is the sign of
//     (ra wb - rb wa) + qa wb sqrt(da) - qb wa sqrt(db),
// that is x + y sqrt(db) with x = (ra wb - rb wa) + qa wb sqrt(da) and
// y = -qb wa: two different radicals at most, which exact::signOf decides.
Sign compareCoordinate(const Dyadic& rationalA, const Dyadic& radicalA, const ExactPoint& a,
                       const Dyadic& rationalB, const Dyadic& radicalB, const ExactPoint& b) {
    const QuadraticNumber x{rationalA * b.weight - rationalB * a.weight, radicalA * b.weight,
                            a.radicand};
    const QuadraticNumber y{-(radicalB * a.weight), Dyadic(), Dyadic()};
    return exact::signOf(x, y, b.radicand);
}

// |c| > K for c = (r + q sqrt(d)) / w with w > 0: c - K > 0 or -c - K > 0,
// that is r - K w + q sqrt(d) > 0 or -r - K w - q sqrt(d) > 0.
bool coordinateBeyond(const Dyadic& rational, const Dyadic& radical, const ExactPoint& point) {
    const Dyadic limit = Dyadic(robust::kMaxCoordinate) * point.weight;
    return exact::signOf(rational - limit, radical, point.radicand) == Sign::positive ||
           exact::signOf(-rational - limit, -radical, point.radicand) == Sign::positive;
}

ExactPoint rationalPoint(ExactVector numerator, Dyadic weight) {
    return ExactPoint{std::move(numerator), ExactVector{}, Dyadic(), std::move(weight)};
}

// The data of a line in a fixed order, so that meet() does not depend on the
// order of its arguments. Two lines with equal keys are the same line, so
// their order does not matter.
bool lineComesFirst(const LineMeaning& a, const LineMeaning& b) noexcept {
    const std::array<double, 4> keyA{a.start.x, a.start.y, a.end.x, a.end.y};
    const std::array<double, 4> keyB{b.start.x, b.start.y, b.end.x, b.end.y};
    // keyA <= keyB; the data are finite, so < is a strict weak order.
    return !std::lexicographical_compare(keyB.begin(), keyB.end(), keyA.begin(), keyA.end());
}

// The same for circles: equal keys mean the same homogeneous data, hence the
// same circle.
bool circleComesFirst(const CircleSupport& a, const CircleSupport& b) noexcept {
    for (const auto order : {compare(a.centerX, b.centerX), compare(a.centerY, b.centerY),
                             compare(a.weight, b.weight),
                             compare(a.radiusSquared, b.radiusSquared)}) {
        if (order != Sign::zero) {
            return order == Sign::negative;
        }
    }
    return true;
}

// Two lines P1 + t U1 and P2 + s U2. Crossing both sides with U2:
//     t cross(U1, U2) = cross(P2 - P1, U2),
// so with den = cross(U1, U2) != 0 the point is
//     (P1 den + U1 cross(P2 - P1, U2)) / den,
// with the signs turned so that the weight is positive. With den == 0 the
// lines are parallel, and the same line when P2 lies on the first one.
CarrierMeeting meetLines(const LineMeaning& first, const LineMeaning& second) {
    const auto start1 = exactVector(first.start);
    const auto direction1 = directionOf(first);
    const auto direction2 = directionOf(second);
    const auto offset = minus(exactVector(second.start), start1);
    CarrierMeeting meeting;
    Dyadic denominator = cross(direction1, direction2);
    if (denominator.isZero()) {
        meeting.sameCarrier = cross(direction1, offset).isZero();
        return meeting;
    }
    ExactVector numerator =
        plus(times(start1, denominator), times(direction1, cross(offset, direction2)));
    if (denominator.sign() == Sign::negative) {
        numerator = negated(numerator);
        denominator = -denominator;
    }
    meeting.count = 1;
    meeting.points[0] = {rationalPoint(std::move(numerator), std::move(denominator)),
                         Contact::crossing};
    return meeting;
}

// The line P + t U and the circle |X - c / w|^2 = rho / w^2. With
// E = w P - c and tau = w t, the points of the line on the circle solve
//     |U|^2 tau^2 + 2 (E . U) tau + |E|^2 - rho = 0,
// whose reduced discriminant is, by Lagrange's identity,
//     disc = (E . U)^2 - |U|^2 (|E|^2 - rho) = rho |U|^2 - cross(E, U)^2.
// With W = w |U|^2 > 0 the points are
//     (P W - U (E . U) -+ U sqrt(disc)) / W,
// the minus root first, since t grows along the line.
CarrierMeeting meetLineCircle(const LineMeaning& line, const CircleSupport& circle) {
    const auto start = exactVector(line.start);
    const auto direction = directionOf(line);
    const auto e = minus(times(start, circle.weight), centerOf(circle));
    const auto directionSquared = dot(direction, direction);
    const auto along = dot(e, direction);
    Dyadic discriminant = circle.radiusSquared * directionSquared - squared(cross(e, direction));
    const auto weight = circle.weight * directionSquared;
    const auto foot = minus(times(start, weight), times(direction, along));
    CarrierMeeting meeting;
    switch (discriminant.sign()) {
        case Sign::negative:
            break;
        case Sign::zero:
            meeting.count = 1;
            meeting.points[0] = {rationalPoint(foot, weight), Contact::touching};
            break;
        case Sign::positive:
            meeting.count = 2;
            meeting.points[0] = {ExactPoint{foot, negated(direction), discriminant, weight},
                                 Contact::crossing};
            meeting.points[1] = {
                ExactPoint{foot, direction, std::move(discriminant), weight}, Contact::crossing};
            break;
    }
    return meeting;
}

// Two circles with centres C1 = c1 / w1, C2 = c2 / w2 and squared radii
// rho1 / w1^2, rho2 / w2^2. With the homogeneous difference of the centres
// D = c2 w1 - c1 w2, so that C2 - C1 = D / (w1 w2), n = |D|^2 and
//     s = n + rho1 w2^2 - rho2 w1^2,
// the radical axis puts the points at C1 + (s / (2 n)) (C2 - C1), plus or
// minus a multiple of the normal: with perp(D) = (-D.y, D.x),
//     X = (2 n w2 c1 + s D +- sqrt(disc) perp(D)) / (2 n w1 w2),
//     disc = 4 n rho1 w2^2 - s^2,
// as |X - C1|^2 = rho1 / w1^2 confirms. disc < 0: no point; disc == 0:
// tangent, inside or outside; disc > 0: two crossings, the right one (the
// minus sign, on the right of the line from C1 to C2) first. n == 0: the same
// centre, and the same circle when the radii are equal.
CarrierMeeting meetCircles(const CircleSupport& first, const CircleSupport& second) {
    const auto center1 = centerOf(first);
    const auto difference =
        minus(times(centerOf(second), first.weight), times(center1, second.weight));
    const auto n = dot(difference, difference);
    const auto radius1 = first.radiusSquared * squared(second.weight);
    const auto radius2 = second.radiusSquared * squared(first.weight);
    CarrierMeeting meeting;
    if (n.isZero()) {
        meeting.sameCarrier = compare(radius1, radius2) == Sign::zero;
        return meeting;
    }
    const auto s = n + radius1 - radius2;
    Dyadic discriminant = (n * radius1).timesPowerOfTwo(2) - squared(s);
    const auto twiceN = n.timesPowerOfTwo(1);
    const auto weight = twiceN * first.weight * second.weight;
    const auto base = plus(times(center1, twiceN * second.weight), times(difference, s));
    switch (discriminant.sign()) {
        case Sign::negative:
            break;
        case Sign::zero:
            meeting.count = 1;
            meeting.points[0] = {rationalPoint(base, weight), Contact::touching};
            break;
        case Sign::positive: {
            const ExactVector right{difference.y, -difference.x};
            meeting.count = 2;
            meeting.points[0] = {ExactPoint{base, right, discriminant, weight}, Contact::crossing};
            meeting.points[1] = {
                ExactPoint{base, negated(right), std::move(discriminant), weight},
                Contact::crossing};
            break;
        }
    }
    return meeting;
}

}  // namespace

ExactPoint exactPoint(Point2 point) {
    return rationalPoint(exactVector(point), Dyadic(1.0));
}

Sign compareX(const ExactPoint& a, const ExactPoint& b) {
    return compareCoordinate(a.rational.x, a.radical.x, a, b.rational.x, b.radical.x, b);
}

Sign compareY(const ExactPoint& a, const ExactPoint& b) {
    return compareCoordinate(a.rational.y, a.radical.y, a, b.rational.y, b.radical.y, b);
}

bool samePoint(const ExactPoint& a, const ExactPoint& b) {
    return compareX(a, b) == Sign::zero && compareY(a, b) == Sign::zero;
}

bool beyondDomain(const ExactPoint& point) {
    return coordinateBeyond(point.rational.x, point.radical.x, point) ||
           coordinateBeyond(point.rational.y, point.radical.y, point);
}

Point2 roundedPoint(const ExactPoint& point) {
    return {exact::quadraticToDouble(point.rational.x, point.radical.x, point.radicand,
                                     point.weight),
            exact::quadraticToDouble(point.rational.y, point.radical.y, point.radicand,
                                     point.weight)};
}

Carrier carrierOf(const CurveMeaning& meaning) {
    assert(!meaning.degenerate && "a degenerate curve has no carrier");
    if (const auto* line = std::get_if<LineMeaning>(&meaning.support)) {
        return *line;
    }
    return std::get<ArcMeaning>(meaning.support).circle;
}

// cross(U, X - P) for X = (r + q sqrt(d)) / w, times w > 0:
//     cross(U, r - P w) + cross(U, q) sqrt(d).
Sign lineSign(const LineMeaning& line, const ExactPoint& point) {
    const auto direction = directionOf(line);
    assert(!(direction.x.isZero() && direction.y.isZero()) && "a line needs start != end");
    const auto offset = minus(point.rational, times(exactVector(line.start), point.weight));
    return exact::signOf(cross(direction, offset), cross(direction, point.radical),
                         point.radicand);
}

// |X - c / w|^2 - rho / w^2 for X = (r + q sqrt(d)) / W, times (w W)^2 > 0:
// with A = w r - c W and B = w q,
//     |A + B sqrt(d)|^2 - rho W^2 = |A|^2 + |B|^2 d - rho W^2 + 2 (A . B) sqrt(d).
Sign circleSign(const CircleSupport& circle, const ExactPoint& point) {
    assert(circle.weight.sign() == Sign::positive && "a circle needs a positive weight");
    const auto a = minus(times(point.rational, circle.weight), times(centerOf(circle), point.weight));
    const auto b = times(point.radical, circle.weight);
    const auto rational =
        dot(a, a) + dot(b, b) * point.radicand - circle.radiusSquared * squared(point.weight);
    return exact::signOf(rational, dot(a, b).timesPowerOfTwo(1), point.radicand);
}

Sign carrierSign(const Carrier& carrier, const ExactPoint& point) {
    if (const auto* line = std::get_if<LineMeaning>(&carrier)) {
        return lineSign(*line, point);
    }
    return circleSign(std::get<CircleSupport>(carrier), point);
}

bool onCarrier(const Carrier& carrier, const ExactPoint& point) {
    return carrierSign(carrier, point) == Sign::zero;
}

Sign sideSign(const CurveMeaning& meaning, const ExactPoint& point) {
    assert(!meaning.degenerate && "a degenerate curve has no side");
    if (const auto* line = std::get_if<LineMeaning>(&meaning.support)) {
        return lineSign(*line, point);
    }
    const auto& arc = std::get<ArcMeaning>(meaning.support);
    // Inside (a negative circleSign) is left for a counterclockwise curve.
    return oriented(arc.turn, negated(circleSign(arc.circle, point)));
}

CarrierMeeting meet(const Carrier& first, const Carrier& second) {
    const auto* line1 = std::get_if<LineMeaning>(&first);
    const auto* line2 = std::get_if<LineMeaning>(&second);
    if (line1 != nullptr && line2 != nullptr) {
        return lineComesFirst(*line1, *line2) ? meetLines(*line1, *line2)
                                              : meetLines(*line2, *line1);
    }
    if (line1 != nullptr) {
        return meetLineCircle(*line1, std::get<CircleSupport>(second));
    }
    if (line2 != nullptr) {
        return meetLineCircle(*line2, std::get<CircleSupport>(first));
    }
    const auto& circle1 = std::get<CircleSupport>(first);
    const auto& circle2 = std::get<CircleSupport>(second);
    return circleComesFirst(circle1, circle2) ? meetCircles(circle1, circle2)
                                              : meetCircles(circle2, circle1);
}

}  // namespace solid_space::geom2d::detail
