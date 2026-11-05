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
//    its bulge +-1, or 0 when the corner and the rounded foot coincide (#152).
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
//    Whatever the direction, the bulge is 0 when the returned points of
//    tangency coincide, while the Arc keeps these rules (tangency.hpp, #152):
//    for a half turn this is checked on the points; for the other directions
//    the angle computed from them is then zero.
// 5. The part of each curve to keep (package P22, decision #184 and
//    #186-#188), decided after every domain diagnosis, with exact signs only.
//    With an arc to continue (rule 1, and rule 3 with distinct points of
//    tangency), t is the direction of the curve at its exact point of tangency
//    T and a the direction in which the exact arc leaves T toward the other
//    point: a is the radius T - C (C the exact centre) turned a quarter turn, counterclockwise at
//    the first point of a counterclockwise arc (and of a half turn) and at the
//    second point of a clockwise one, clockwise otherwise. The sign of
//    dot(t, a) is computed in the tower of the candidate, from the exact T and
//    C; for a circular curve t is its own radius T - O turned in its direction
//    of travel. For the semicircle the arc leaves both points in the same
//    direction, away from the first curve (its points are symmetric about the
//    centre), so the signs are dot products of dyadic directions. A straight
//    curve that would turn back at T gives radiusTooLarge (#187).
//    Without an arc (rule 2, and rule 3 with the same exact point of tangency
//    twice), the corner P is an exact point of the tower: for rule 2 the
//    common point of the carriers that intersect() returns, found again with
//    meet() in the same exact order. Straight curves and Circles use the sign
//    of dot(pick - P, t). Arcs and curved pieces compare angles measured from
//    their start D0 in their direction of travel (curve_meaning.hpp, sectorOf
//    and compareTurns): the pick direction lies in the part from the start to
//    P when its angle is at most that of P, and in the part from P to the end
//    when it is at least that of P and at most that of D1 (or, with P beyond
//    the end, when it is at least that of P or at most that of D1; a whole
//    arc ends at D0, at a full turn). Two angles measured from different
//    directions, the extensions D1 -> P and P -> D0 of #188, are compared as
//    the directions (r . v, cross(r, v)), each the angle from r to v seen from
//    (1, 0): one more product, no root.
//
// Constructions beyond the domain (curves.hpp, decision #149). tangentCandidates()
// gives the diagnosis outOfDomain about the radius (input 2) when an exact
// centre or point of tangency of any candidate lies beyond
// robust::kMaxCoordinate (DECISIONI, 2026-11-01: a tangent circle is never
// left out silently), and both functions pass it on. In the same way a corner
// of rule 2 that intersect() omits beyond the domain gives that diagnosis:
// "none" would claim that no corner exists. Rule 1 checks its exact centre,
// foot and radius: the foot lies beyond the domain when the corner is near it
// and the second line runs out of it. Rules 1 and 3 then check the point m of
// the bulge form, reconstructed exactly from the returned points and bulge
// (curves.hpp), not from the exact arc: near the boundary either outcome is
// permitted (#155, #157), and this is the one the returned doubles give. The
// boundary rule needs no clamping: every returned coordinate and the radius of
// rule 1 are correctly rounded, non-decreasing in the exact value, and
// robust::kMaxCoordinate is a double, so an exact value within it rounds to a
// double within it.
//
// The arc in numbers (ADR 0011, points 6 and 7). With the rounded centre C and
// points of tangency T1, T2 (each coordinate correctly rounded), v1 = T1 - C
// and v2 = T2 - C in double; the start angle is robust::atan2(v1), the sweep
// robust::atan2(|cross(v1, v2)|, v1 . v2), the products taken on v1 and v2
// scaled by the same power of two (so that nothing underflows for a tiny
// radius), with the sign of the exact
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

#include <algorithm>
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

using detail::ArcMeaning;
using detail::ArcSpan;
using detail::CircleSupport;
using detail::CurveMeaning;
using detail::Dyadic;
using detail::ExactPoint;
using detail::ExactVector;
using detail::LineMeaning;
using detail::TangentCandidate;
using detail::Tower;
using detail::TowerNumber;
using detail::TowerPoint;
using detail::Turn;
using detail::TurnSector;
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

// True when the returned points of tangency are the same point (-0 and +0
// alike): the bulge form is then 0, while the Arc keeps its rules (#152).
bool coincide(Point2 a, Point2 b) noexcept {
    return a.x == b.x && a.y == b.y;
}

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
            return {Arc{centre, radius, start, kHalfTurn}, coincide(from, to) ? 0.0 : 1.0};
        case Turning::counterclockwise:
        case Turning::clockwise:
            break;
    }
    const double toX = to.x - centre.x;
    const double toY = to.y - centre.y;
    // The products are taken on the vectors scaled by a power of two so that
    // their largest coordinate lies in [1/2, 1): for a tiny radius they would
    // otherwise underflow to (signed) zeros and atan2 would give a wrong angle
    // (issue found in the review of #123). The scaling changes neither the
    // angle nor the bits of an exact product; a coordinate that becomes
    // subnormal moves by at most 2^-1075, far below u against the largest.
    const double largest = std::fmax(std::fmax(std::fabs(fromX), std::fabs(fromY)),
                                     std::fmax(std::fabs(toX), std::fabs(toY)));
    int exponent = 0;
    static_cast<void>(std::frexp(largest, &exponent));
    const double ax = std::ldexp(fromX, -exponent);
    const double ay = std::ldexp(fromY, -exponent);
    const double bx = std::ldexp(toX, -exponent);
    const double by = std::ldexp(toY, -exponent);
    const double cross = (ax * by) - (ay * bx);
    const double dot = (ax * bx) + (ay * by);
    // Both zero only when a rounded point of tangency is the rounded centre,
    // that is the radius is below the declared error; then the signs of the
    // zeros would make atan2 give pi or 0 at random.
    const double magnitude =
        cross == 0.0 && dot == 0.0 ? 0.0 : robust::atan2(std::fabs(cross), dot);
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

// The diagnosis of a construction beyond the domain (input 2).
Diagnosis constructionBeyond(const char* message) {
    return Diagnosis{DiagnosisCode::outOfDomain, kRadiusInput, message};
}

bool beyondCoordinate(const Dyadic& value) {
    return compare(value.abs(), Dyadic(robust::kMaxCoordinate)) == Sign::positive;
}

// True when the point m of the bulge form from `from` to `to`, reconstructed
// exactly from these doubles, lies beyond the domain. A zero bulge has no m.
bool middleBeyondDomain(Point2 from, Point2 to, double bulge) {
    if (bulge == 0.0) {
        return false;
    }
    const auto m = detail::arcMidpoint(PolylineSegment{from, to, bulge});
    return beyondCoordinate(m.x) || beyondCoordinate(m.y);
}

constexpr const char* kMiddleBeyond =
    "the point m of the bulge form of the fillet is beyond robust::kMaxCoordinate";

// --- Rule 5: the part of each curve to keep ----------------------------------

ExactVector directionOf(const LineMeaning& line) {
    const auto start = detail::exactVector(line.start);
    const auto end = detail::exactVector(line.end);
    return {end.x - start.x, end.y - start.y};
}

// A vector whose coordinates are numbers of a tower.
struct TowerVector {
    TowerNumber x;
    TowerNumber y;
};

TowerVector towerVector(const ExactVector& vector) {
    return {TowerNumber(vector.x), TowerNumber(vector.y)};
}

// The vector turned a quarter turn counterclockwise.
TowerVector quarterTurn(const TowerVector& vector) {
    return {-vector.y, vector.x};
}

TowerVector negated(const TowerVector& vector) {
    return {-vector.x, -vector.y};
}

TowerNumber dot(const Tower& tower, const TowerVector& a, const TowerVector& b) {
    return tower.product(a.x, b.x) + tower.product(a.y, b.y);
}

TowerNumber cross(const Tower& tower, const TowerVector& a, const TowerVector& b) {
    return tower.product(a.x, b.y) - tower.product(a.y, b.x);
}

bool isZero(const Tower& tower, const TowerVector& vector) {
    return tower.sign(vector.x) == Sign::zero && tower.sign(vector.y) == Sign::zero;
}

// a - b, scaled by the product of their (positive) weights.
TowerVector difference(const TowerPoint& a, const TowerPoint& b) {
    return {(a.x * b.weight) - (b.x * a.weight), (a.y * b.weight) - (b.y * a.weight)};
}

// The point minus the centre of the circle, scaled by positive weights.
TowerVector fromCentre(const TowerPoint& point, const CircleSupport& circle) {
    return {(point.x * circle.weight) - TowerNumber(circle.centerX * point.weight),
            (point.y * circle.weight) - TowerNumber(circle.centerY * point.weight)};
}

// The direction t of a curve at a point of its carrier, scaled by a positive
// factor: from the start to the end of a straight curve; on a circular one its
// radius turned a quarter turn in its direction of travel (a Circle runs
// counterclockwise).
TowerVector directionAt(const CurveMeaning& curve, const TowerPoint& point) {
    if (const auto* line = std::get_if<LineMeaning>(&curve.support)) {
        return towerVector(directionOf(*line));
    }
    const auto& arc = std::get<ArcMeaning>(curve.support);
    const auto tangent = quarterTurn(fromCentre(point, arc.circle));
    return arc.turn == Turn::counterclockwise ? tangent : negated(tangent);
}

// The part that continues the arc, from the sign of dot(t, a). The two
// directions are parallel and not zero, so the sign is never zero.
KeptPart continuing(Sign dotOfDirections) {
    assert(dotOfDirections != Sign::zero && "t and a are parallel");
    return dotOfDirections == Sign::positive ? KeptPart::fromStart : KeptPart::toEnd;
}

// With an arc to continue: the part of a curve whose exact point of tangency
// is `touching`, for the exact centre of the arc and the direction in which the
// arc leaves that point, counterclockwise or clockwise around the centre.
KeptPart continuingAt(const Tower& tower, const CurveMeaning& curve, const TowerPoint& touching,
                      const TowerPoint& centre, Turn leaving) {
    const auto radius = quarterTurn(difference(touching, centre));
    const auto away = leaving == Turn::counterclockwise ? radius : negated(radius);
    return continuing(tower.sign(dot(tower, directionAt(curve, touching), away)));
}

// True when a straight curve would turn back at its point of tangency to
// continue the arc (#187): beforeStart with fromStart, afterEnd with toEnd.
bool turnsBack(const CurveMeaning& curve, Location location, KeptPart part) {
    if (!std::holds_alternative<LineMeaning>(curve.support)) {
        return false;
    }
    return (location == Location::beforeStart && part == KeptPart::fromStart) ||
           (location == Location::afterEnd && part == KeptPart::toEnd);
}

// The CHAMFER rule: toEnd when dot(pick - P, t) is positive, fromStart when it
// is negative, nothing (ambiguousPick) at zero.
std::optional<KeptPart> keptBySign(Sign dotOfPick) {
    switch (dotOfPick) {
        case Sign::positive:
            return KeptPart::toEnd;
        case Sign::negative:
            return KeptPart::fromStart;
        case Sign::zero:
            break;
    }
    return std::nullopt;
}

// The sector of a direction from the start D0 of an arc, in its direction of
// travel. Precondition: the direction is not zero.
TurnSector sectorFromStart(const Tower& tower, const ArcMeaning& arc,
                           const TowerVector& direction) {
    const auto start = towerVector(arc.startDirection);
    return detail::sectorOf(detail::oriented(arc.turn, tower.sign(cross(tower, start, direction))),
                            tower.sign(dot(tower, start, direction)));
}

// A direction with its sector from D0.
struct Angle {
    TowerVector direction;
    TurnSector sector = TurnSector::zero;
};

// The sign of angle(u) - angle(v), both measured from D0 in the direction of
// the arc, in [0, 2 pi).
Sign compareFromStart(const Tower& tower, const ArcMeaning& arc, const Angle& u, const Angle& v) {
    if (u.sector != v.sector) {
        return detail::compareTurns(u.sector, v.sector, Sign::zero);
    }
    return detail::compareTurns(
        u.sector, v.sector,
        detail::oriented(arc.turn, tower.sign(cross(tower, u.direction, v.direction))));
}

// The angle from r to v in the direction of the arc, written as the direction
// that has that angle from (1, 0) counterclockwise: (r . v, cross(r, v)), the
// cross product oriented in the direction of the arc. Its length is
// |r| |v|, which does not change its angle.
TowerVector angleBetween(const Tower& tower, const ArcMeaning& arc, const TowerVector& r,
                         const TowerVector& v) {
    const auto turned = cross(tower, r, v);
    return {dot(tower, r, v), arc.turn == Turn::counterclockwise ? turned : -turned};
}

// The sign of angle(u) - angle(v) for two directions written by angleBetween(),
// both measured from (1, 0) counterclockwise in [0, 2 pi).
Sign compareAngles(const Tower& tower, const TowerVector& u, const TowerVector& v) {
    const auto sectorOf = [&tower](const TowerVector& w) {
        return detail::sectorOf(tower.sign(w.y), tower.sign(w.x));
    };
    const auto su = sectorOf(u);
    const auto sv = sectorOf(v);
    if (su != sv) {
        return detail::compareTurns(su, sv, Sign::zero);
    }
    return detail::compareTurns(su, sv, tower.sign(cross(tower, u, v)));
}

// The rule of #188 for an arc or a curved piece: the part, along its circle,
// that contains the direction of the pick from the centre. `corner` and
// `pick` are directions from the centre; the corner is not zero.
std::optional<KeptPart> keptOnArc(const Tower& tower, const ArcMeaning& arc,
                                  const TowerVector& corner, const TowerVector& pick) {
    if (isZero(tower, pick)) {
        return std::nullopt;
    }
    const Angle atCorner{corner, sectorFromStart(tower, arc, corner)};
    const Angle atPick{pick, sectorFromStart(tower, arc, pick)};
    const Sign pickFromCorner = compareFromStart(tower, arc, atPick, atCorner);
    // The part from the start to P: angles from 0 to that of P.
    const bool inFromStart = pickFromCorner != Sign::positive;
    bool inToEnd = false;
    bool cornerBeyond = false;
    if (arc.span == ArcSpan::whole) {
        // From P to the end, which is D0 after a full turn.
        inToEnd = pickFromCorner != Sign::negative || atPick.sector == TurnSector::zero;
    } else {
        assert(arc.span == ArcSpan::partial);
        const Angle atEnd{towerVector(arc.endDirection), arc.endSector};
        const bool pickOnArc = compareFromStart(tower, arc, atPick, atEnd) != Sign::positive;
        cornerBeyond = compareFromStart(tower, arc, atCorner, atEnd) == Sign::positive;
        // From P to the end: back to D0 and on to D1 when P is beyond the end.
        inToEnd = cornerBeyond ? (pickFromCorner != Sign::negative || pickOnArc)
                               : (pickFromCorner != Sign::negative && pickOnArc);
    }
    if (inFromStart && inToEnd) {
        if (!cornerBeyond) {
            return std::nullopt;
        }
        // The smaller extension: the end moves from D1 to P (fromStart), or
        // the start back from D0 to P (toEnd); equal ones give nothing.
        const auto towardCorner = angleBetween(tower, arc, towerVector(arc.endDirection), corner);
        const auto backToStart = angleBetween(tower, arc, corner, towerVector(arc.startDirection));
        switch (compareAngles(tower, towardCorner, backToStart)) {
            case Sign::negative:
                return KeptPart::fromStart;
            case Sign::positive:
                return KeptPart::toEnd;
            case Sign::zero:
                return std::nullopt;
        }
    }
    if (inFromStart) {
        return KeptPart::fromStart;
    }
    if (inToEnd) {
        return KeptPart::toEnd;
    }
    return std::nullopt;
}

// Without an arc to continue: the part of a curve shown by its pick with
// respect to the exact corner (a number of `tower`); nothing when the pick
// does not show a unique part (ambiguousPick).
std::optional<KeptPart> keptByPick(const Tower& tower, const CurveMeaning& curve,
                                   const TowerPoint& corner, Point2 pick) {
    const auto picked = detail::towerPoint(pick);
    const auto* arc = std::get_if<ArcMeaning>(&curve.support);
    if (arc == nullptr || arc->span == ArcSpan::circle) {
        return keptBySign(
            tower.sign(dot(tower, difference(picked, corner), directionAt(curve, corner))));
    }
    return keptOnArc(tower, *arc, fromCentre(corner, arc->circle), fromCentre(picked, arc->circle));
}

// A fillet that cannot be made without turning a straight curve back (#187).
Fillet tooLarge() {
    Fillet result;
    result.outcome = TangencyOutcome::radiusTooLarge;
    return result;
}

// A fillet whose picks do not show a part to keep.
Fillet ambiguous() {
    Fillet result;
    result.outcome = TangencyOutcome::ambiguousPick;
    return result;
}

// Sets the parts of a found fillet with an arc to continue, or tells that a
// straight curve would turn back.
Result<Fillet> withContinuingParts(Fillet result, const CurveMeaning& first,
                                   const CurveMeaning& second, KeptPart keepFirst,
                                   KeptPart keepSecond) {
    if (turnsBack(first, result.onFirst.position.location, keepFirst) ||
        turnsBack(second, result.onSecond.position.location, keepSecond)) {
        return tooLarge();
    }
    result.keepFirst = keepFirst;
    result.keepSecond = keepSecond;
    return result;
}

// Sets the parts of a found fillet without an arc, from the picks.
Result<Fillet> withPickedParts(Fillet result, const Tower& tower, const CurveMeaning& first,
                               const TowerPoint& onFirst, Point2 pickFirst,
                               const CurveMeaning& second, const TowerPoint& onSecond,
                               Point2 pickSecond) {
    const auto keepFirst = keptByPick(tower, first, onFirst, pickFirst);
    const auto keepSecond = keptByPick(tower, second, onSecond, pickSecond);
    if (!keepFirst.has_value() || !keepSecond.has_value()) {
        return ambiguous();
    }
    result.keepFirst = *keepFirst;
    result.keepSecond = *keepSecond;
    return result;
}

// --- Rule 1: the semicircle ---------------------------------------------------

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

Result<Fillet> semicircle(const CurveMeaning& first, const LineMeaning& firstLine,
                          const CurveMeaning& second, const LineMeaning& secondLine,
                          Point2 pickFirst) {
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
    // The exact values, before rounding (#149): the radius |cross| / (2 sqrt(L))
    // is beyond K exactly when cross^2 > 4 K^2 L.
    if (detail::beyondDomain(centre)) {
        return constructionBeyond("the centre of the semicircle is beyond robust::kMaxCoordinate");
    }
    if (detail::beyondDomain(foot)) {
        return constructionBeyond(
            "the foot of the perpendicular on the second line is beyond robust::kMaxCoordinate");
    }
    const Dyadic limit(robust::kMaxCoordinate);
    if (compare(cross * cross, limit * limit * squaredLength.timesPowerOfTwo(2)) ==
        Sign::positive) {
        return constructionBeyond("the radius of the semicircle is beyond robust::kMaxCoordinate");
    }
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
    if (!coincide(result.onFirst.point, result.onSecond.point)) {
        result.bulge = counterclockwise ? 1.0 : -1.0;
    }
    if (middleBeyondDomain(result.onFirst.point, result.onSecond.point, result.bulge)) {
        return constructionBeyond(kMiddleBeyond);
    }
    // Rule 5: the arc leaves both E and F in the direction `away` (F is the
    // reflection of E through the centre, where the direction of travel is
    // reversed, and leaving F toward E reverses it again).
    return withContinuingParts(result, first, second, atEnd ? KeptPart::fromStart : KeptPart::toEnd,
                               continuing(detail::dotSign(direction, away)));
}

// --- Rule 2: the corner -------------------------------------------------------

// The exact common points of the two extended carriers, in the order of
// intersect() with Extension::extended: every point of meet() takes part, and
// they are sorted exactly along the first curve (intersection.cpp). Only when
// the carriers are not the same and no point is beyond the domain.
std::vector<ExactPoint> exactCorners(const CurveMeaning& first, const CurveMeaning& second) {
    const auto meeting = detail::meet(detail::carrierOf(first), detail::carrierOf(second));
    std::vector<ExactPoint> points;
    for (const auto& point : meeting.found()) {
        points.push_back(point.point);
    }
    std::sort(points.begin(), points.end(), [&first](const ExactPoint& a, const ExactPoint& b) {
        return detail::compareAlong(first, a, b) == Sign::negative;
    });
    return points;
}

Result<Fillet> corner(const CurveMeaning& firstMeaning, const CurveMeaning& secondMeaning,
                      const Curve& first, Point2 pickFirst, const Curve& second,
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
    const auto index = nearestOf(found.points.size(), pickFirst, pickSecond, [&](std::size_t i) {
        const Point2 p = found.points[i].point;
        return std::pair<Point2, Point2>{p, p};
    });
    const auto& point = found.points[index];
    result.outcome = TangencyOutcome::found;
    result.center = point.point;
    result.radius = 0.0;
    result.onFirst = TangencyPoint{point.point, point.onFirst};
    result.onSecond = TangencyPoint{point.point, point.onSecond};
    result.arc = Arc{point.point, 0.0, 0.0, 0.0};
    result.bulge = 0.0;

    // Rule 5, without an arc: the exact corner.
    const auto corners = exactCorners(firstMeaning, secondMeaning);
    assert(corners.size() == found.points.size() && "the same points as intersect()");
    const auto& exact = corners[index];
    assert(detail::nearestPoint(exact).x == point.point.x &&
           detail::nearestPoint(exact).y == point.point.y && "the same corner as intersect()");
    Tower tower;
    const auto exactCorner = detail::towerPoint(tower, exact);
    if (!exactCorner.has_value()) {
        // An empty tower always has room for the one root of an exact point.
        assert(false && "an empty tower takes one root");
        return Diagnosis{DiagnosisCode::outOfDomain, kRadiusInput,
                         "the exact corner needs more roots than a tower holds"};
    }
    return withPickedParts(result, tower, firstMeaning, *exactCorner, pickFirst, secondMeaning,
                           *exactCorner, pickSecond);
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
    const Turning turning = turningOf(chosen);
    const auto arc =
        arcBetween(circle.center, radius, circle.onFirst.point, circle.onSecond.point, turning);
    result.outcome = TangencyOutcome::found;
    result.center = circle.center;
    result.radius = radius;
    result.onFirst = circle.onFirst;
    result.onSecond = circle.onSecond;
    result.arc = arc.arc;
    result.bulge = arc.bulge;
    if (middleBeyondDomain(result.onFirst.point, result.onSecond.point, result.bulge)) {
        return constructionBeyond(kMiddleBeyond);
    }
    // Rule 5.
    const auto& tower = chosen.tower;
    if (turning == Turning::none) {
        return withPickedParts(result, tower, firstMeaning, chosen.onFirst, pickFirst,
                               secondMeaning, chosen.onSecond, pickSecond);
    }
    // A half turn is counterclockwise (rule 4). The arc leaves the second
    // point toward the first against its direction of travel.
    const Turn atFirst = turning == Turning::clockwise ? Turn::clockwise : Turn::counterclockwise;
    const Turn atSecond =
        atFirst == Turn::counterclockwise ? Turn::clockwise : Turn::counterclockwise;
    return withContinuingParts(
        result, firstMeaning, secondMeaning,
        continuingAt(tower, firstMeaning, chosen.onFirst, chosen.center, atFirst),
        continuingAt(tower, secondMeaning, chosen.onSecond, chosen.center, atSecond));
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
        return corner(meaningOfFirst, meaningOfSecond, first, pickFirst, second, pickSecond);
    }
    return tangentFillet(meaningOfFirst, meaningOfSecond, first, pickFirst, second, pickSecond,
                         radius);
}

}  // namespace solid_space::geom2d
