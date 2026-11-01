// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// tangentCircleNear and fillet of tangency.hpp (stage 0.2, package P16;
// ADR 0011, points 4-7, 9 and 11). Both choose among the tangent circles of
// tangency.cpp (tangency_internal.hpp, tangentCandidates), which keep their
// exact centres and points of tangency, so that every decision below is exact
// and every rounded value is the one tangentCircles() returns.
//
// The choice near the picks (ADR 0011, point 4, and the alternatives). The
// expression of tangency.hpp, and nothing else: on the rounded points,
//     ((dx1 * dx1) + (dy1 * dy1)) + ((dx2 * dx2) + (dy2 * dy2)),
// each difference and operation rounded once (no contraction, ADR 0008,
// point 5), with (dx1, dy1) = onFirst.point - nearFirst and (dx2, dy2) =
// onSecond.point - nearSecond. Coordinates stay below 2^52, so nothing
// overflows. The smallest value wins, and a tie keeps the earlier circle.
//
// The rules of fillet, in the order of tangency.hpp:
// 1. Two straight curves on parallel lines, decided exactly (the cross product
//    of their directions is zero): the same line (the start of the first lies
//    on the second line, orient2d) gives infinitelyMany; otherwise the
//    semicircle. Its corner E is the end of the first curve nearer the pick,
//    the squared distances compared exactly (a tie goes to the end). With S and
//    U the start and the direction of the second line and L = |U|^2, the foot
//    is F = S + U ((E - S) . U) / L and the centre (E + F) / 2: rational
//    points, correctly rounded. The radius |E - F| / 2 is
//    |cross(U, E - S)| / (2 sqrt(L)), correctly rounded. The semicircle leaves
//    E away from the first curve, in the direction of the first curve at its
//    end and against it at its start; since E - F is perpendicular to both
//    lines, the arc turns counterclockwise exactly when
//    cross(E - S, away) > 0. Its sweep is +-pi (the double nearest to it) and
//    its bulge +-1.
// 2. Radius zero: the common points of the two extended curves (intersect()),
//    chosen as tangentCircleNear chooses. A shared stretch gives
//    infinitelyMany, no common point none.
// 3. Otherwise the candidates of tangentCandidates() whose sides are those of
//    side() for the picks; ambiguousPick when a pick lies on the line or circle
//    of the other curve, checked before the circles are computed.
// 4. The arc of the chosen circle. Its direction is the sign of
//    cross(T1 - P, T2 - P) for the exact centre P and points of tangency T1,
//    T2, computed in the tower of the candidate: positive is counterclockwise.
//    When it is zero, the sign of the dot product tells the same point (the
//    single point, sweep 0, bulge 0) from the opposite ends of a diameter
//    (counterclockwise, sweep pi, bulge 1): both points lie on the circle.
//
// Centres and corners beyond the domain. tangentCandidates() gives the
// diagnosis outOfDomain about the radius (input 2) when a centre lies beyond
// robust::kMaxCoordinate (DECISIONI, 2026-11-01: a tangent circle is never
// left out silently), and both functions pass it on. In the same way a corner
// of rule 2 that intersect() omits beyond the domain gives that diagnosis:
// "none" would claim that no corner exists.
//
// The arc in numbers (ADR 0011, points 6 and 7). With the rounded centre C and
// points of tangency T1, T2 (each coordinate correctly rounded), v1 = T1 - C
// and v2 = T2 - C in double; the start angle is robust::atan2(v1), the sweep
// robust::atan2(|cross(v1, v2)|, v1 . v2) with the sign of the exact
// direction, so its magnitude never exceeds pi; the bulge is
// tan(sweep / 4) = sine / cosine of robust::sinCos(sweep / 4). Error budget,
// with u = 2^-53 and P = max(M, |p|): each coordinate of v1 and v2 is within
// 4 u P of the exact one (the two roundings of the points and that of the
// difference), so their directions are within about 10 u P / r of the exact
// ones; atan2 and sinCos add at most 2 ulp of angles below 2 pi, the sum
// startAngle + sweep one rounding. The ends of the Arc are then within about
// 60 u P of the exact points of tangency, and the middle of the bulge form
// within about 20 u P of the middle of the exact arc: both below
// kRelativeArcError * P = 2^-46 P = 128 u P. The semicircle has the same
// budget, with sweep +-pi exact up to the rounding of pi.
//
// All the computation is here (ADR 0008, point 5): robust::atan2 and
// robust::sinCos, no trigonometry of <cmath>.

#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "exact/dyadic.hpp"
#include "exact/tower.hpp"
#include "offset_carriers.hpp"
#include "positions.hpp"
#include "solid_space/geom2d/intersection.hpp"
#include "solid_space/geom2d/side.hpp"
#include "solid_space/geom2d/tangency.hpp"
#include "solid_space/robust/point.hpp"
#include "solid_space/robust/predicates.hpp"
#include "solid_space/robust/trigonometry.hpp"
#include "tangency_internal.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <utility>
#include <variant>
#include <vector>

namespace solid_space::geom2d {
namespace {

using detail::CurveMeaning;
using detail::Dyadic;
using detail::ExactPoint;
using detail::ExactVector;
using detail::LineMeaning;
using detail::TangentCandidate;
using detail::TowerNumber;
using detail::TowerPoint;
using robust::Sign;

// The inputs of tangency.hpp.
constexpr std::size_t kFirstInput = 0;
constexpr std::size_t kSecondInput = 1;
constexpr std::size_t kRadiusInput = 2;
constexpr std::size_t kFirstPickInput = 3;
constexpr std::size_t kSecondPickInput = 4;

// The double nearest to pi: the sweep of a semicircle.
constexpr double kHalfTurn = std::numbers::pi;

// What the radius may be: tangentCircleNear rejects zero (as tangentCircles),
// fillet accepts it (rule 2).
enum class ZeroRadius : std::uint8_t { rejected, accepted };

std::optional<Diagnosis> checkPick(Point2 pick, std::size_t input) {
    if (!std::isfinite(pick.x) || !std::isfinite(pick.y)) {
        return Diagnosis{DiagnosisCode::invalidValue, input,
                         "a coordinate of the point is NaN or infinite"};
    }
    if (std::fabs(pick.x) > robust::kMaxCoordinate || std::fabs(pick.y) > robust::kMaxCoordinate) {
        return Diagnosis{DiagnosisCode::outOfDomain, input,
                         "a coordinate of the point is beyond robust::kMaxCoordinate"};
    }
    return std::nullopt;
}

// The diagnoses of tangency.hpp about the radius and the picks, in the order
// of the inputs; the curves are checked first, by checkedMeaning().
std::optional<Diagnosis> checkOthers(double radius, ZeroRadius zero, Point2 pickFirst,
                                     Point2 pickSecond) {
    if (const auto diagnosis = detail::checkRadius(radius, kRadiusInput)) {
        return diagnosis;
    }
    if (zero == ZeroRadius::rejected && radius == 0.0) {
        return Diagnosis{DiagnosisCode::degenerateCurve, kRadiusInput,
                         "a tangent circle of radius zero is a single point"};
    }
    if (const auto diagnosis = checkPick(pickFirst, kFirstPickInput)) {
        return diagnosis;
    }
    return checkPick(pickSecond, kSecondPickInput);
}

// The nearness of tangency.hpp (see the top of this file).
double nearness(Point2 onFirst, Point2 nearFirst, Point2 onSecond, Point2 nearSecond) {
    const double dx1 = onFirst.x - nearFirst.x;
    const double dy1 = onFirst.y - nearFirst.y;
    const double dx2 = onSecond.x - nearSecond.x;
    const double dy2 = onSecond.y - nearSecond.y;
    return ((dx1 * dx1) + (dy1 * dy1)) + ((dx2 * dx2) + (dy2 * dy2));
}

// The index of the nearest of `count` > 0 choices, the earlier one on a tie.
// pointsOf(i) gives the two points of tangency of choice i.
template <typename PointsOf>
std::size_t nearestOf(std::size_t count, Point2 nearFirst, Point2 nearSecond,
                      const PointsOf& pointsOf) {
    assert(count > 0);
    std::size_t best = 0;
    double bestValue = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const auto [onFirst, onSecond] = pointsOf(i);
        const double value = nearness(onFirst, nearFirst, onSecond, nearSecond);
        if (i == 0 || value < bestValue) {
            best = i;
            bestValue = value;
        }
    }
    return best;
}

// --- The arc ------------------------------------------------------------------

// How the fillet arc runs from the first point of tangency to the second.
enum class Turning : std::uint8_t {
    counterclockwise,
    clockwise,
    // The two points are the same exact point.
    none,
    // The two points are opposite ends of a diameter: counterclockwise.
    halfTurn,
};

// The two forms of the arc.
struct FilletArc {
    Arc arc;
    double bulge = 0.0;
};

// The arc of radius `radius` around the rounded centre from `from` to `to`
// (see the top of this file).
FilletArc arcBetween(Point2 centre, double radius, Point2 from, Point2 to, Turning turning) {
    const double fromX = from.x - centre.x;
    const double fromY = from.y - centre.y;
    const double start = robust::atan2(fromY, fromX);
    switch (turning) {
        case Turning::none:
            return {Arc{centre, radius, start, 0.0}, 0.0};
        case Turning::halfTurn:
            return {Arc{centre, radius, start, kHalfTurn}, 1.0};
        case Turning::counterclockwise:
        case Turning::clockwise:
            break;
    }
    const double toX = to.x - centre.x;
    const double toY = to.y - centre.y;
    const double cross = (fromX * toY) - (fromY * toX);
    const double dot = (fromX * toX) + (fromY * toY);
    const double magnitude = robust::atan2(std::fabs(cross), dot);
    if (!(magnitude > 0.0)) {
        // The points are too close for the doubles to tell an angle: the arc
        // is shorter than the declared error.
        return {Arc{centre, radius, start, 0.0}, 0.0};
    }
    const double sweep = turning == Turning::counterclockwise ? magnitude : -magnitude;
    const auto quarter = robust::sinCos(sweep / 4.0);
    return {Arc{centre, radius, start, sweep}, quarter.sine / quarter.cosine};
}

// The direction of the arc of a tangent circle, decided exactly in its tower.
Turning turningOf(const TangentCandidate& candidate) {
    const auto& tower = candidate.tower;
    const auto& centre = candidate.center;
    // T - P scaled by the two weights, both positive.
    const auto fromCentre = [&](const TowerPoint& point) {
        return std::pair<TowerNumber, TowerNumber>{
            (point.x * centre.weight) - (centre.x * point.weight),
            (point.y * centre.weight) - (centre.y * point.weight)};
    };
    const auto [ax, ay] = fromCentre(candidate.onFirst);
    const auto [bx, by] = fromCentre(candidate.onSecond);
    const Sign cross = tower.compare(tower.product(ax, by), tower.product(ay, bx));
    switch (cross) {
        case Sign::positive:
            return Turning::counterclockwise;
        case Sign::negative:
            return Turning::clockwise;
        case Sign::zero:
            break;
    }
    const Sign dot = tower.sign(tower.product(ax, bx) + tower.product(ay, by));
    return dot == Sign::positive ? Turning::none : Turning::halfTurn;
}

// --- Rule 1: the semicircle ---------------------------------------------------

ExactVector directionOf(const LineMeaning& line) {
    const auto start = detail::exactVector(line.start);
    const auto end = detail::exactVector(line.end);
    return {end.x - start.x, end.y - start.y};
}

Dyadic squaredDistance(Point2 a, Point2 b) {
    const auto exactA = detail::exactVector(a);
    const auto exactB = detail::exactVector(b);
    const Dyadic dx = exactA.x - exactB.x;
    const Dyadic dy = exactA.y - exactB.y;
    return dx * dx + dy * dy;
}

bool parallel(const LineMeaning& a, const LineMeaning& b) {
    return detail::crossSign(directionOf(a), directionOf(b)) == Sign::zero;
}

Fillet semicircle(const CurveMeaning& first, const LineMeaning& firstLine,
                  const CurveMeaning& second, const LineMeaning& secondLine, Point2 pickFirst) {
    Fillet result;
    if (robust::orient2d(secondLine.start, secondLine.end, firstLine.start) == Sign::zero) {
        result.outcome = TangencyOutcome::infinitelyMany;
        return result;
    }
    const bool atEnd = compare(squaredDistance(firstLine.start, pickFirst),
                               squaredDistance(firstLine.end, pickFirst)) != Sign::negative;
    const Point2 corner = atEnd ? firstLine.end : firstLine.start;
    const auto exactCorner = detail::exactVector(corner);
    const auto origin = detail::exactVector(secondLine.start);
    const auto direction = directionOf(secondLine);
    const Dyadic squaredLength = direction.x * direction.x + direction.y * direction.y;
    const ExactVector relative{exactCorner.x - origin.x, exactCorner.y - origin.y};
    const Dyadic along = relative.x * direction.x + relative.y * direction.y;
    // F = (S L + U along) / L, and the centre (E L + S L + U along) / (2 L).
    const ExactVector footNumerator{origin.x * squaredLength + direction.x * along,
                                    origin.y * squaredLength + direction.y * along};
    const ExactPoint foot{footNumerator, ExactVector{}, Dyadic(), squaredLength};
    const ExactPoint centre{ExactVector{exactCorner.x * squaredLength + footNumerator.x,
                                        exactCorner.y * squaredLength + footNumerator.y},
                            ExactVector{}, Dyadic(), squaredLength.timesPowerOfTwo(1)};
    const Dyadic cross = direction.x * relative.y - direction.y * relative.x;
    const double radius =
        detail::correctlyRounded(cross.abs(), Dyadic(), Dyadic(), Dyadic(2.0), squaredLength);

    const auto firstDirection = directionOf(firstLine);
    const ExactVector away =
        atEnd ? firstDirection : ExactVector{-firstDirection.x, -firstDirection.y};
    const bool counterclockwise = detail::crossSign(relative, away) == Sign::positive;

    result.outcome = TangencyOutcome::found;
    result.center = detail::nearestPoint(centre);
    result.radius = radius;
    result.onFirst = TangencyPoint{corner, detail::positionOn(first, detail::exactPoint(corner))};
    result.onSecond = TangencyPoint{detail::nearestPoint(foot), detail::positionOn(second, foot)};
    result.semicircle = true;
    const double start = robust::atan2(corner.y - result.center.y, corner.x - result.center.x);
    result.arc = Arc{result.center, radius, start, counterclockwise ? kHalfTurn : -kHalfTurn};
    result.bulge = counterclockwise ? 1.0 : -1.0;
    return result;
}

// --- Rule 2: the corner -------------------------------------------------------

Result<Fillet> corner(const Curve& first, Point2 pickFirst, const Curve& second,
                      Point2 pickSecond) {
    const auto common = intersect(first, Extension::extended, second, Extension::extended);
    if (!common.hasValue()) {
        // The data were checked already.
        return common.diagnosis();
    }
    const auto& found = common.value();
    Fillet result;
    if (!found.overlaps.empty()) {
        result.outcome = TangencyOutcome::infinitelyMany;
        return result;
    }
    if (found.omittedBeyondDomain > 0) {
        return Diagnosis{DiagnosisCode::outOfDomain, kRadiusInput,
                         "a common point of the extended curves is beyond "
                         "robust::kMaxCoordinate"};
    }
    if (found.points.empty()) {
        result.outcome = TangencyOutcome::none;
        return result;
    }
    const auto& point =
        found.points[nearestOf(found.points.size(), pickFirst, pickSecond, [&](std::size_t i) {
            const Point2 p = found.points[i].point;
            return std::pair<Point2, Point2>{p, p};
        })];
    result.outcome = TangencyOutcome::found;
    result.center = point.point;
    result.radius = 0.0;
    result.onFirst = TangencyPoint{point.point, point.onFirst};
    result.onSecond = TangencyPoint{point.point, point.onSecond};
    result.arc = Arc{point.point, 0.0, 0.0, 0.0};
    result.bulge = 0.0;
    return result;
}

// --- Rule 3: the tangent circle -----------------------------------------------

// side() of side.hpp. The curve and the pick were checked already, so it gives
// a value.
Side sideOfPick(const Curve& curve, Point2 pick) {
    const auto result = side(curve, pick);
    assert(result.hasValue() && "the curve and the pick were checked");
    return result.hasValue() ? result.value() : Side::on;
}

Result<Fillet> tangentFillet(const CurveMeaning& firstMeaning, const CurveMeaning& secondMeaning,
                             const Curve& first, Point2 pickFirst, const Curve& second,
                             Point2 pickSecond, double radius) {
    Fillet result;
    const Side sideOfFirst = sideOfPick(first, pickSecond);
    const Side sideOfSecond = sideOfPick(second, pickFirst);
    if (sideOfFirst == Side::on || sideOfSecond == Side::on) {
        result.outcome = TangencyOutcome::ambiguousPick;
        return result;
    }
    auto candidates = detail::tangentCandidates(firstMeaning, secondMeaning, radius);
    if (!candidates.hasValue()) {
        return candidates.diagnosis();
    }
    if (candidates.value().infinitelyMany) {
        result.outcome = TangencyOutcome::infinitelyMany;
        return result;
    }
    std::vector<const TangentCandidate*> onSides;
    for (const auto& candidate : candidates.value().candidates) {
        if (candidate.circle.sideOfFirst == sideOfFirst &&
            candidate.circle.sideOfSecond == sideOfSecond) {
            onSides.push_back(&candidate);
        }
    }
    if (onSides.empty()) {
        result.outcome = TangencyOutcome::none;
        return result;
    }
    const auto& chosen =
        *onSides[nearestOf(onSides.size(), pickFirst, pickSecond, [&](std::size_t i) {
            const auto& circle = onSides[i]->circle;
            return std::pair<Point2, Point2>{circle.onFirst.point, circle.onSecond.point};
        })];
    const auto& circle = chosen.circle;
    const auto arc = arcBetween(circle.center, radius, circle.onFirst.point, circle.onSecond.point,
                                turningOf(chosen));
    result.outcome = TangencyOutcome::found;
    result.center = circle.center;
    result.radius = radius;
    result.onFirst = circle.onFirst;
    result.onSecond = circle.onSecond;
    result.arc = arc.arc;
    result.bulge = arc.bulge;
    return result;
}

}  // namespace

Result<ChosenCircle> tangentCircleNear(const Curve& first, Point2 nearFirst, const Curve& second,
                                       Point2 nearSecond, double radius) {
    auto firstMeaning = detail::checkedMeaning(first, kFirstInput);
    if (!firstMeaning.hasValue()) {
        return firstMeaning.diagnosis();
    }
    auto secondMeaning = detail::checkedMeaning(second, kSecondInput);
    if (!secondMeaning.hasValue()) {
        return secondMeaning.diagnosis();
    }
    if (const auto diagnosis = checkOthers(radius, ZeroRadius::rejected, nearFirst, nearSecond)) {
        return *diagnosis;
    }
    auto candidates =
        detail::tangentCandidates(firstMeaning.value(), secondMeaning.value(), radius);
    if (!candidates.hasValue()) {
        return candidates.diagnosis();
    }
    const auto& found = candidates.value();
    ChosenCircle result;
    if (found.infinitelyMany) {
        result.outcome = TangencyOutcome::infinitelyMany;
        return result;
    }
    if (found.candidates.empty()) {
        result.outcome = TangencyOutcome::none;
        return result;
    }
    const auto index =
        nearestOf(found.candidates.size(), nearFirst, nearSecond, [&](std::size_t i) {
            const auto& circle = found.candidates[i].circle;
            return std::pair<Point2, Point2>{circle.onFirst.point, circle.onSecond.point};
        });
    result.outcome = TangencyOutcome::found;
    result.circle = found.candidates[index].circle;
    return result;
}

Result<Fillet> fillet(const Curve& first, Point2 pickFirst, const Curve& second, Point2 pickSecond,
                      double radius) {
    auto firstMeaning = detail::checkedMeaning(first, kFirstInput);
    if (!firstMeaning.hasValue()) {
        return firstMeaning.diagnosis();
    }
    auto secondMeaning = detail::checkedMeaning(second, kSecondInput);
    if (!secondMeaning.hasValue()) {
        return secondMeaning.diagnosis();
    }
    if (const auto diagnosis = checkOthers(radius, ZeroRadius::accepted, pickFirst, pickSecond)) {
        return *diagnosis;
    }
    const auto& meaningOfFirst = firstMeaning.value();
    const auto& meaningOfSecond = secondMeaning.value();
    const auto* firstLine = std::get_if<LineMeaning>(&meaningOfFirst.support);
    const auto* secondLine = std::get_if<LineMeaning>(&meaningOfSecond.support);
    if (firstLine != nullptr && secondLine != nullptr && parallel(*firstLine, *secondLine)) {
        return semicircle(meaningOfFirst, *firstLine, meaningOfSecond, *secondLine, pickFirst);
    }
    if (radius == 0.0) {
        return corner(first, pickFirst, second, pickSecond);
    }
    return tangentFillet(meaningOfFirst, meaningOfSecond, first, pickFirst, second, pickSecond,
                         radius);
}

}  // namespace solid_space::geom2d
