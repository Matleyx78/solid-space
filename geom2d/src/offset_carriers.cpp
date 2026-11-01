// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Offset carriers and their exact meetings (offset_carriers.hpp; stage 0.2,
// package P12).
//
// The formulas are those of carriers.cpp, written for offset data: a line is
// cross(U, X) = c with a rational direction U and a constant c of the tower,
// a circle keeps its rational centre c / w and gets a squared radius
// rho / w^2 of the tower. So the weights of the meetings stay rational and
// positive, and only the constants, the squared radii and the discriminants
// carry roots. The derivations are written next to each formula.

#include "offset_carriers.hpp"

#include "exact/quadratic.hpp"
#include "solid_space/robust/point.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <optional>
#include <utility>
#include <variant>

namespace solid_space::geom2d::detail {
namespace {

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

Dyadic dot(const ExactVector& a, const ExactVector& b) {
    return a.x * b.x + a.y * b.y;
}

Dyadic cross(const ExactVector& a, const ExactVector& b) {
    return a.x * b.y - a.y * b.x;
}

// A vector with coordinates of the tower.
struct TowerVector {
    TowerNumber x;
    TowerNumber y;
};

TowerNumber squaredNorm(const Tower& tower, const TowerVector& vector) {
    return tower.product(vector.x, vector.x) + tower.product(vector.y, vector.y);
}

// The weight w scaled out of a point: (x w' - c W, y w' - c W) is the vector
// from the centre c / w' to the point (x, y) / W, times W w' > 0.
TowerVector fromCenter(const OffsetCircle& circle, const TowerPoint& point) {
    return {point.x * circle.weight - TowerNumber(circle.centerX * point.weight),
            point.y * circle.weight - TowerNumber(circle.centerY * point.weight)};
}

// The vector u - origin, times Wu W0 > 0.
TowerVector fromOrigin(const TowerPoint& origin, const TowerPoint& point) {
    return {point.x * origin.weight - origin.x * point.weight,
            point.y * origin.weight - origin.y * point.weight};
}

Sign crossSign(const Tower& tower, const TowerVector& a, const TowerVector& b) {
    return tower.compare(tower.product(a.x, b.y), tower.product(a.y, b.x));
}

Sign dotSign(const Tower& tower, const TowerVector& a, const TowerVector& b) {
    return tower.sign(tower.product(a.x, b.x) + tower.product(a.y, b.y));
}

// The lexicographic order of the representations of two numbers, used only to
// put the data of two carriers in a fixed order.
Sign compareRepresentations(const TowerNumber& a, const TowerNumber& b) {
    const auto coefficientsA = a.coefficients();
    const auto coefficientsB = b.coefficients();
    if (coefficientsA.size() != coefficientsB.size()) {
        return coefficientsA.size() < coefficientsB.size() ? Sign::negative : Sign::positive;
    }
    for (std::size_t i = 0; i < coefficientsA.size(); ++i) {
        const Sign order = compare(coefficientsA[i], coefficientsB[i]);
        if (order != Sign::zero) {
            return order;
        }
    }
    return Sign::zero;
}

// The data of a line in a fixed order, so that meetOffset() does not depend on
// the order of its arguments. Equal data are the same line.
bool lineComesFirst(const OffsetLine& a, const OffsetLine& b) {
    for (const auto order :
         {compare(a.direction.x, b.direction.x), compare(a.direction.y, b.direction.y),
          compareRepresentations(a.constant, b.constant)}) {
        if (order != Sign::zero) {
            return order == Sign::negative;
        }
    }
    return true;
}

// The same for circles, by centre only: two circles with points in common
// have different centres, and equal centre data mean the same centre.
bool circleComesFirst(const OffsetCircle& a, const OffsetCircle& b) {
    for (const auto order : {compare(a.centerX, b.centerX), compare(a.centerY, b.centerY),
                             compare(a.weight, b.weight)}) {
        if (order != Sign::zero) {
            return order == Sign::negative;
        }
    }
    return true;
}

TowerPoint pointOf(TowerNumber x, TowerNumber y, Dyadic weight) {
    return TowerPoint{std::move(x), std::move(y), std::move(weight)};
}

// Two lines cross(U1, X) = c1 and cross(U2, X) = c2, that is
// -U1.y x + U1.x y = c1 and -U2.y x + U2.x y = c2. With
// D = cross(U1, U2) != 0, Cramer's rule gives
//     x = (c1 U2.x - c2 U1.x) / D,   y = (c1 U2.y - c2 U1.y) / D,
// with the signs turned so that the weight is positive. With D == 0 the lines
// are parallel, U2 = lambda U1 with lambda = (U1 . U2) / |U1|^2, and the same
// line when c2 = lambda c1, that is (U1 . U2) c1 - |U1|^2 c2 = 0.
OffsetMeeting meetLines(const Tower& tower, const OffsetLine& first, const OffsetLine& second) {
    const auto& u1 = first.direction;
    const auto& u2 = second.direction;
    OffsetMeeting meeting;
    Dyadic denominator = cross(u1, u2);
    if (denominator.isZero()) {
        meeting.sameCarrier =
            tower.sign(first.constant * dot(u1, u2) - second.constant * dot(u1, u1)) == Sign::zero;
        return meeting;
    }
    TowerNumber x = first.constant * u2.x - second.constant * u1.x;
    TowerNumber y = first.constant * u2.y - second.constant * u1.y;
    if (denominator.sign() == Sign::negative) {
        x = -x;
        y = -y;
        denominator = -denominator;
    }
    meeting.count = 1;
    meeting.points[0] = {pointOf(std::move(x), std::move(y), std::move(denominator)),
                         Contact::crossing};
    return meeting;
}

// The line cross(U, X) = c and the circle |X - C / w|^2 = rho / w^2, with
// L = |U|^2 and the left normal N = (-U.y, U.x), so that cross(U, N) = L and
// N . U = 0. With G = c w - cross(U, C) and
//     disc = L rho - G^2,
// the points are
//     X = (L C + N G -+ U sqrt(disc)) / (L w),
// the minus root first, since the position grows along U: cross(U, X) L w =
// L cross(U, C) + L G = L c w, and |X - C / w|^2 = (L G^2 + L disc) / (L w)^2
// = rho / w^2. disc < 0: no point; disc == 0: tangent; disc > 0: two
// crossings.
std::optional<OffsetMeeting> meetLineCircle(Tower& tower, const OffsetLine& line,
                                            const OffsetCircle& circle) {
    const auto& u = line.direction;
    const ExactVector center{circle.centerX, circle.centerY};
    const auto squaredLength = dot(u, u);
    const TowerNumber g = line.constant * circle.weight - TowerNumber(cross(u, center));
    const TowerNumber discriminant = circle.radiusSquared * squaredLength - tower.product(g, g);
    const auto weight = squaredLength * circle.weight;
    const TowerNumber baseX = TowerNumber(squaredLength * center.x) - g * u.y;
    const TowerNumber baseY = TowerNumber(squaredLength * center.y) + g * u.x;
    OffsetMeeting meeting;
    switch (tower.sign(discriminant)) {
        case Sign::negative:
            break;
        case Sign::zero:
            meeting.count = 1;
            meeting.points[0] = {pointOf(baseX, baseY, weight), Contact::touching};
            break;
        case Sign::positive: {
            const auto root = tower.squareRoot(discriminant);
            if (!root) {
                return std::nullopt;
            }
            meeting.count = 2;
            meeting.points[0] = {pointOf(baseX - *root * u.x, baseY - *root * u.y, weight),
                                 Contact::crossing};
            meeting.points[1] = {pointOf(baseX + *root * u.x, baseY + *root * u.y, weight),
                                 Contact::crossing};
            break;
        }
    }
    return meeting;
}

// Two circles with centres C1 = c1 / w1, C2 = c2 / w2 and squared radii
// rho1 / w1^2, rho2 / w2^2, as meetCircles() of carriers.cpp: with
// D = c2 w1 - c1 w2, n = |D|^2, R1 = rho1 w2^2, R2 = rho2 w1^2 and
// s = n + R1 - R2,
//     X = (2 n w2 c1 + s D +- sqrt(disc) perp(D)) / (2 n w1 w2),
//     disc = 4 n R1 - s^2,
// the right point (perp(D) = (D.y, -D.x)) first. n == 0: the same centre, and
// the same circle when R1 == R2 (both radii are positive).
std::optional<OffsetMeeting> meetCircles(Tower& tower, const OffsetCircle& first,
                                         const OffsetCircle& second) {
    const ExactVector difference{second.centerX * first.weight - first.centerX * second.weight,
                                 second.centerY * first.weight - first.centerY * second.weight};
    const auto n = dot(difference, difference);
    const TowerNumber radius1 = first.radiusSquared * (second.weight * second.weight);
    const TowerNumber radius2 = second.radiusSquared * (first.weight * first.weight);
    OffsetMeeting meeting;
    if (n.isZero()) {
        meeting.sameCarrier = tower.compare(radius1, radius2) == Sign::zero;
        return meeting;
    }
    const TowerNumber s = TowerNumber(n) + radius1 - radius2;
    const TowerNumber discriminant = (radius1 * n).timesPowerOfTwo(2) - tower.product(s, s);
    const auto twiceN = n.timesPowerOfTwo(1);
    const auto weight = twiceN * first.weight * second.weight;
    const auto scale = twiceN * second.weight;
    const TowerNumber baseX = TowerNumber(first.centerX * scale) + s * difference.x;
    const TowerNumber baseY = TowerNumber(first.centerY * scale) + s * difference.y;
    switch (tower.sign(discriminant)) {
        case Sign::negative:
            break;
        case Sign::zero:
            meeting.count = 1;
            meeting.points[0] = {pointOf(baseX, baseY, weight), Contact::touching};
            break;
        case Sign::positive: {
            const auto root = tower.squareRoot(discriminant);
            if (!root) {
                return std::nullopt;
            }
            // right = (D.y, -D.x)
            meeting.count = 2;
            meeting.points[0] = {
                pointOf(baseX + *root * difference.y, baseY - *root * difference.x, weight),
                Contact::crossing};
            meeting.points[1] = {
                pointOf(baseX - *root * difference.y, baseY + *root * difference.x, weight),
                Contact::crossing};
            break;
        }
    }
    return meeting;
}

}  // namespace

TowerPoint towerPoint(Point2 point) {
    const auto exact = exactVector(point);
    return TowerPoint{TowerNumber(exact.x), TowerNumber(exact.y), Dyadic(1.0)};
}

// (r + q sqrt(d)) / w with sqrt(d) a number of the tower.
std::optional<TowerPoint> towerPoint(Tower& tower, const ExactPoint& point) {
    TowerPoint result{TowerNumber(point.rational.x), TowerNumber(point.rational.y), point.weight};
    if (point.radical.x.isZero() && point.radical.y.isZero()) {
        return result;
    }
    const auto root = tower.squareRoot(TowerNumber(point.radicand));
    if (!root) {
        return std::nullopt;
    }
    result.x = result.x + *root * point.radical.x;
    result.y = result.y + *root * point.radical.y;
    return result;
}

// The sign of sqrt(rho) - d w (counterclockwise) or sqrt(rho) + d w
// (clockwise): a + b sqrt(rho) with b = 1, which exact::signOf decides.
Sign offsetRadiusSign(const ArcMeaning& arc, double distance) {
    assert(arc.span != ArcSpan::point && "a single point has no radius to move");
    const Dyadic shift = Dyadic(distance) * arc.circle.weight;
    const Dyadic rational = arc.turn == Turn::counterclockwise ? -shift : shift;
    return exact::signOf(rational, Dyadic(1.0), arc.circle.radiusSquared);
}

// A line from P to Q moved by d to its left: cross(U, P') = cross(U, P) + d |U|
// for P' = P + d (-U.y, U.x) / |U|, since cross(U, (-U.y, U.x)) = |U|^2.
OffsetCarrierResult offsetCarrier(Tower& tower, const CurveMeaning& meaning, double distance) {
    assert(!meaning.degenerate && "a degenerate curve has no carrier");
    const Dyadic shift(distance);
    if (const auto* line = std::get_if<LineMeaning>(&meaning.support)) {
        const auto start = exactVector(line->start);
        const auto end = exactVector(line->end);
        OffsetLine result{ExactVector{end.x - start.x, end.y - start.y}, TowerNumber()};
        result.constant = TowerNumber(cross(result.direction, start));
        if (shift.isZero()) {
            return {OffsetStatus::done, std::move(result)};
        }
        const auto length = tower.squareRoot(TowerNumber(dot(result.direction, result.direction)));
        if (!length) {
            return {OffsetStatus::towerFull, std::move(result)};
        }
        result.constant = result.constant + *length * shift;
        return {OffsetStatus::done, std::move(result)};
    }
    const auto& arc = std::get<ArcMeaning>(meaning.support);
    OffsetCircle result{arc.circle.centerX, arc.circle.centerY, arc.circle.weight,
                        TowerNumber(arc.circle.radiusSquared), arc.turn};
    if (shift.isZero()) {
        return {OffsetStatus::done, std::move(result)};
    }
    if (offsetRadiusSign(arc, distance) != Sign::positive) {
        return {OffsetStatus::vanished, std::move(result)};
    }
    const auto radius = tower.squareRoot(TowerNumber(arc.circle.radiusSquared));
    if (!radius) {
        return {OffsetStatus::towerFull, std::move(result)};
    }
    // Counterclockwise, left is inside: the radius shrinks by d.
    const Dyadic change = shift * arc.circle.weight;
    const TowerNumber moved = arc.turn == Turn::counterclockwise ? *radius - TowerNumber(change)
                                                                 : *radius + TowerNumber(change);
    result.radiusSquared = tower.product(moved, moved);
    return {OffsetStatus::done, std::move(result)};
}

std::optional<OffsetMeeting> meetOffset(Tower& tower, const OffsetCarrier& first,
                                        const OffsetCarrier& second) {
    const auto* line1 = std::get_if<OffsetLine>(&first);
    const auto* line2 = std::get_if<OffsetLine>(&second);
    if (line1 != nullptr && line2 != nullptr) {
        return lineComesFirst(*line1, *line2) ? meetLines(tower, *line1, *line2)
                                              : meetLines(tower, *line2, *line1);
    }
    if (line1 != nullptr) {
        return meetLineCircle(tower, *line1, std::get<OffsetCircle>(second));
    }
    if (line2 != nullptr) {
        return meetLineCircle(tower, *line2, std::get<OffsetCircle>(first));
    }
    const auto& circle1 = std::get<OffsetCircle>(first);
    const auto& circle2 = std::get<OffsetCircle>(second);
    return circleComesFirst(circle1, circle2) ? meetCircles(tower, circle1, circle2)
                                              : meetCircles(tower, circle2, circle1);
}

// Line: cross(U, X) - c for X = (x, y) / W, times W > 0:
//     U.x y - U.y x - c W.
// Circle: |X - C / w|^2 - rho / w^2, times (w W)^2 > 0:
//     |(x w - C.x W, y w - C.y W)|^2 - rho W^2.
Sign carrierSign(const Tower& tower, const OffsetCarrier& carrier, const TowerPoint& point) {
    if (const auto* line = std::get_if<OffsetLine>(&carrier)) {
        return tower.sign(point.y * line->direction.x - point.x * line->direction.y -
                          line->constant * point.weight);
    }
    const auto& circle = std::get<OffsetCircle>(carrier);
    return tower.sign(squaredNorm(tower, fromCenter(circle, point)) -
                      circle.radiusSquared * (point.weight * point.weight));
}

bool onCarrier(const Tower& tower, const OffsetCarrier& carrier, const TowerPoint& point) {
    return carrierSign(tower, carrier, point) == Sign::zero;
}

Sign sideSign(const Tower& tower, const OffsetCarrier& carrier, const TowerPoint& point) {
    const Sign sign = carrierSign(tower, carrier, point);
    if (const auto* circle = std::get_if<OffsetCircle>(&carrier)) {
        // Inside (a negative sign) is left for a counterclockwise circle.
        return oriented(circle->turn, negated(sign));
    }
    return sign;
}

// Line: U . (u / Wu - v / Wv), times Wu Wv > 0. Circle: the directions from
// the centre, placed by the sectors of curve_meaning.hpp from the direction of
// `from`, in the direction of the circle.
Sign compareAlong(const Tower& tower, const OffsetCarrier& carrier, const TowerPoint& from,
                  const TowerPoint& u, const TowerPoint& v) {
    if (const auto* line = std::get_if<OffsetLine>(&carrier)) {
        const TowerNumber dx = u.x * v.weight - v.x * u.weight;
        const TowerNumber dy = u.y * v.weight - v.y * u.weight;
        return tower.sign(dx * line->direction.x + dy * line->direction.y);
    }
    const auto& circle = std::get<OffsetCircle>(carrier);
    const auto reference = fromCenter(circle, from);
    const auto directionU = fromCenter(circle, u);
    const auto directionV = fromCenter(circle, v);
    const TurnSector sectorU =
        sectorOf(oriented(circle.turn, crossSign(tower, reference, directionU)),
                 dotSign(tower, reference, directionU));
    const TurnSector sectorV =
        sectorOf(oriented(circle.turn, crossSign(tower, reference, directionV)),
                 dotSign(tower, reference, directionV));
    // The cross product of u and v is read only inside one quarter.
    const Sign crossUV = sectorU == sectorV
                             ? oriented(circle.turn, crossSign(tower, directionU, directionV))
                             : Sign::zero;
    return compareTurns(sectorU, sectorV, crossUV);
}

// |u - o|^2 - |v - o|^2 with u - o = U / (Wu Wo) and v - o = V / (Wv Wo):
// times (Wu Wv Wo)^2 > 0, |U|^2 Wv^2 - |V|^2 Wu^2.
Sign compareDistances(const Tower& tower, const TowerPoint& origin, const TowerPoint& u,
                      const TowerPoint& v) {
    const TowerNumber nearU = squaredNorm(tower, fromOrigin(origin, u)) * (v.weight * v.weight);
    const TowerNumber nearV = squaredNorm(tower, fromOrigin(origin, v)) * (u.weight * u.weight);
    return tower.compare(nearU, nearV);
}

bool samePoint(const Tower& tower, const TowerPoint& a, const TowerPoint& b) {
    return tower.compare(a.x * b.weight, b.x * a.weight) == Sign::zero &&
           tower.compare(a.y * b.weight, b.y * a.weight) == Sign::zero;
}

// |c| > K for c = x / W with W > 0: x - K W > 0 or -x - K W > 0.
bool beyondDomain(const Tower& tower, const TowerPoint& point) {
    const TowerNumber limit(Dyadic(robust::kMaxCoordinate) * point.weight);
    const auto beyond = [&](const TowerNumber& coordinate) {
        return tower.sign(coordinate - limit) == Sign::positive ||
               tower.sign(-coordinate - limit) == Sign::positive;
    };
    return beyond(point.x) || beyond(point.y);
}

Point2 roundedPoint(const Tower& tower, const TowerPoint& point) {
    return {tower.quotientToDouble(point.x, point.weight),
            tower.quotientToDouble(point.y, point.weight)};
}

}  // namespace solid_space::geom2d::detail
