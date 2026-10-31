// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// The exact meaning of the curves (curve_meaning.hpp; stage 0.2, package P7).

#include "curve_meaning.hpp"

#include "solid_space/robust/point.hpp"
#include "solid_space/robust/trigonometry.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <initializer_list>
#include <string>
#include <utility>

namespace solid_space::geom2d::detail {
namespace {

// The limits of the domain (curves.hpp, "Checking the data").
constexpr double kMaxCoordinate = robust::kMaxCoordinate;
constexpr double kMaxAngle = robust::kMaxTrigArgument;

// Overload set for std::visit.
template <typename... Functions>
struct Overloaded : Functions... {
    using Functions::operator()...;
};

Diagnosis diagnosis(DiagnosisCode code, std::size_t input, std::string message) {
    return Diagnosis{code, input, std::move(message)};
}

bool allFinite(std::initializer_list<double> values) {
    return std::all_of(values.begin(), values.end(),
                       [](double value) { return std::isfinite(value); });
}

bool anyBeyond(std::initializer_list<double> values, double limit) {
    return std::any_of(values.begin(), values.end(),
                       [limit](double value) { return std::fabs(value) > limit; });
}

bool beyondCoordinate(const Dyadic& value) {
    return compare(value.abs(), Dyadic(kMaxCoordinate)) == Sign::positive;
}

// Exact comparison of two points of doubles: -0 and +0 are the same.
bool samePoint(Point2 a, Point2 b) noexcept {
    return a.x == b.x && a.y == b.y;
}

// Straight PolylineSegment: bulge exactly zero (-0 included).
bool isCurved(const PolylineSegment& piece) noexcept {
    return piece.bulge != 0.0;
}

Dyadic square(const Dyadic& value) {
    return value * value;
}

std::optional<Diagnosis> invalidValue(std::size_t input, const char* what) {
    return diagnosis(DiagnosisCode::invalidValue, input, what);
}

std::optional<Diagnosis> outOfDomain(std::size_t input, const char* what) {
    return diagnosis(DiagnosisCode::outOfDomain, input, what);
}

constexpr const char* kNotFinite = "a value of the curve is NaN or infinite";
constexpr const char* kNegativeRadius = "the radius of the curve is negative";
constexpr const char* kCoordinateBeyond =
    "a coordinate or radius of the curve is beyond robust::kMaxCoordinate";

std::optional<Diagnosis> checkSegment(const Segment& segment, std::size_t input) {
    const auto values = {segment.start.x, segment.start.y, segment.end.x, segment.end.y};
    if (!allFinite(values)) {
        return invalidValue(input, kNotFinite);
    }
    if (anyBeyond(values, kMaxCoordinate)) {
        return outOfDomain(input, kCoordinateBeyond);
    }
    return std::nullopt;
}

std::optional<Diagnosis> checkCircle(const Circle& circle, std::size_t input) {
    const auto values = {circle.center.x, circle.center.y, circle.radius};
    if (!allFinite(values)) {
        return invalidValue(input, kNotFinite);
    }
    if (circle.radius < 0.0) {
        return invalidValue(input, kNegativeRadius);
    }
    if (anyBeyond(values, kMaxCoordinate)) {
        return outOfDomain(input, kCoordinateBeyond);
    }
    return std::nullopt;
}

std::optional<Diagnosis> checkArc(const Arc& arc, std::size_t input) {
    if (!allFinite({arc.center.x, arc.center.y, arc.radius, arc.startAngle, arc.sweep})) {
        return invalidValue(input, kNotFinite);
    }
    if (arc.radius < 0.0) {
        return invalidValue(input, kNegativeRadius);
    }
    if (anyBeyond({arc.center.x, arc.center.y, arc.radius}, kMaxCoordinate)) {
        return outOfDomain(input, kCoordinateBeyond);
    }
    // The end angle as curves.hpp defines it: the sum rounded to double.
    const double endAngle = arc.startAngle + arc.sweep;
    if (anyBeyond({arc.startAngle, endAngle}, kMaxAngle)) {
        return outOfDomain(input, "an angle of the arc is beyond robust::kMaxTrigArgument");
    }
    return std::nullopt;
}

std::optional<Diagnosis> checkPiece(const PolylineSegment& piece, std::size_t input) {
    const auto coordinates = {piece.start.x, piece.start.y, piece.end.x, piece.end.y};
    if (!allFinite(coordinates) || !std::isfinite(piece.bulge)) {
        return invalidValue(input, kNotFinite);
    }
    if (anyBeyond(coordinates, kMaxCoordinate)) {
        return outOfDomain(input, kCoordinateBeyond);
    }
    if (isCurved(piece)) {
        const auto m = arcMidpoint(piece);
        if (beyondCoordinate(m.x) || beyondCoordinate(m.y)) {
            return outOfDomain(input,
                               "the point m of the curved polyline segment is beyond "
                               "robust::kMaxCoordinate");
        }
    }
    return std::nullopt;
}

double largestMagnitude(std::initializer_list<double> values) {
    double result = 0.0;
    for (const double value : values) {
        result = std::max(result, std::fabs(value));
    }
    return result;
}

CurveMeaning lineMeaning(const Curve& curve, Point2 start, Point2 end) {
    return CurveMeaning{curve, magnitudeOf(curve), samePoint(start, end), LineMeaning{start, end}};
}

CurveMeaning circleMeaning(const Circle& circle) {
    const ExactVector start{Dyadic(1.0), Dyadic(0.0)};
    ArcMeaning arc{CircleSupport{Dyadic(circle.center.x), Dyadic(circle.center.y), Dyadic(1.0),
                                 square(Dyadic(circle.radius))},
                   Turn::counterclockwise,
                   ArcSpan::circle,
                   start,
                   start,
                   TurnSector::zero};
    return CurveMeaning{circle, magnitudeOf(circle), circle.radius == 0.0, std::move(arc)};
}

CurveMeaning arcMeaning(const Arc& arc) {
    const auto start = robust::sinCos(arc.startAngle);
    const auto end = robust::sinCos(arc.startAngle + arc.sweep);
    const Point2 d0{start.cosine, start.sine};
    const Point2 d1{end.cosine, end.sine};
    const auto rule = arcRule(arc.sweep, d0, d1);
    ArcMeaning meaning{CircleSupport{Dyadic(arc.center.x), Dyadic(arc.center.y), Dyadic(1.0),
                                     square(Dyadic(arc.radius))},
                       rule.turn,
                       rule.span,
                       exactVector(d0),
                       exactVector(rule.span == ArcSpan::whole ? d0 : d1),
                       rule.endSector};
    const bool degenerate = arc.radius == 0.0 || rule.span == ArcSpan::point;
    return CurveMeaning{arc, magnitudeOf(arc), degenerate, std::move(meaning)};
}

CurveMeaning pieceMeaning(const PolylineSegment& piece) {
    if (!isCurved(piece)) {
        return lineMeaning(piece, piece.start, piece.end);
    }
    // The circle through start, m and end, in homogeneous coordinates (see
    // CircleSupport): centre (2 b (start + end) + (b^2 - 1) (dy, -dx)) / (4 b),
    // radius |chord| (1 + b^2) / (4 |b|). Both signs of the weight describe
    // the same centre; it is made positive.
    const Dyadic bulge(piece.bulge);
    const auto start = exactVector(piece.start);
    const auto end = exactVector(piece.end);
    const Dyadic dx = end.x - start.x;
    const Dyadic dy = end.y - start.y;
    const Dyadic twiceBulge = bulge.timesPowerOfTwo(1);
    const Dyadic bulgeSquared = square(bulge);
    const Dyadic minusOne = bulgeSquared - Dyadic(1.0);
    const Dyadic plusOne = bulgeSquared + Dyadic(1.0);
    Dyadic centerX = twiceBulge * (start.x + end.x) + minusOne * dy;
    Dyadic centerY = twiceBulge * (start.y + end.y) - minusOne * dx;
    Dyadic weight = bulge.timesPowerOfTwo(2);
    const Turn turn = bulge.sign() == Sign::positive ? Turn::counterclockwise : Turn::clockwise;
    if (turn == Turn::clockwise) {
        centerX = -centerX;
        centerY = -centerY;
        weight = -weight;
    }
    CircleSupport circle{centerX, centerY, weight, (square(dx) + square(dy)) * square(plusOne)};

    ArcMeaning arc{std::move(circle), turn, ArcSpan::point, {}, {}, TurnSector::zero};
    const bool degenerate = samePoint(piece.start, piece.end);
    if (!degenerate) {
        // Directions from the centre, scaled by the positive weight.
        arc.startDirection = ExactVector{start.x * arc.circle.weight - arc.circle.centerX,
                                         start.y * arc.circle.weight - arc.circle.centerY};
        arc.endDirection = ExactVector{end.x * arc.circle.weight - arc.circle.centerX,
                                       end.y * arc.circle.weight - arc.circle.centerY};
        // The arc through m turns from start to end by 4 atan(|b|), strictly
        // between zero and a whole turn: it is always partial.
        arc.span = ArcSpan::partial;
        arc.endSector = sectorOf(oriented(turn, crossSign(arc.startDirection, arc.endDirection)),
                                 dotSign(arc.startDirection, arc.endDirection));
    }
    return CurveMeaning{piece, magnitudeOf(piece), degenerate, std::move(arc)};
}

// The sector index, in the order of the angles.
int sectorIndex(TurnSector sector) noexcept {
    return static_cast<int>(sector);
}

bool isOpenQuarter(TurnSector sector) noexcept {
    return sectorIndex(sector) % 2 == 1;
}

Sign signOfDifference(int a, int b) noexcept {
    if (a < b) {
        return Sign::negative;
    }
    return a > b ? Sign::positive : Sign::zero;
}

Sign negate(Sign sign) noexcept {
    return static_cast<Sign>(-static_cast<int>(sign));
}

}  // namespace

ExactVector exactVector(Point2 point) {
    return ExactVector{Dyadic(point.x), Dyadic(point.y)};
}

Sign crossSign(const ExactVector& a, const ExactVector& b) {
    return compare(a.x * b.y, a.y * b.x);
}

Sign dotSign(const ExactVector& a, const ExactVector& b) {
    return compare(a.x * b.x, -(a.y * b.y));
}

Sign oriented(Turn turn, Sign sign) noexcept {
    return turn == Turn::counterclockwise ? sign : negate(sign);
}

TurnSector sectorOf(Sign orientedCross, Sign dot) noexcept {
    assert(orientedCross != Sign::zero || dot != Sign::zero);
    switch (orientedCross) {
        case Sign::zero:
            return dot == Sign::positive ? TurnSector::zero : TurnSector::half;
        case Sign::positive:
            if (dot == Sign::zero) {
                return TurnSector::quarter;
            }
            return dot == Sign::positive ? TurnSector::first : TurnSector::second;
        case Sign::negative:
            if (dot == Sign::zero) {
                return TurnSector::threeQuarters;
            }
            return dot == Sign::positive ? TurnSector::fourth : TurnSector::third;
    }
    return TurnSector::zero;
}

Sign compareTurns(TurnSector su, TurnSector sv, Sign orientedCrossUV) noexcept {
    if (su != sv) {
        return signOfDifference(sectorIndex(su), sectorIndex(sv));
    }
    if (!isOpenQuarter(su)) {
        return Sign::zero;
    }
    // Both strictly inside the same quarter: u comes first exactly when v
    // turns from u in the direction of travel.
    return negate(orientedCrossUV);
}

std::optional<Diagnosis> checkData(const Curve& curve, std::size_t input) {
    return std::visit(
        Overloaded{
            [input](const Segment& segment) { return checkSegment(segment, input); },
            [input](const Arc& arc) { return checkArc(arc, input); },
            [input](const Circle& circle) { return checkCircle(circle, input); },
            [input](const PolylineSegment& piece) { return checkPiece(piece, input); },
        },
        curve);
}

std::optional<Diagnosis> check(const Curve& curve, std::size_t input) {
    if (auto problem = checkData(curve, input)) {
        return problem;
    }
    if (meaningOf(curve).degenerate) {
        return degenerateDiagnosis(input);
    }
    return std::nullopt;
}

Diagnosis degenerateDiagnosis(std::size_t input) {
    return diagnosis(DiagnosisCode::degenerateCurve, input, "the curve is exactly a single point");
}

ExactVector arcMidpoint(const PolylineSegment& piece) {
    const auto start = exactVector(piece.start);
    const auto end = exactVector(piece.end);
    const Dyadic bulge(piece.bulge);
    return ExactVector{
        (start.x + end.x + bulge * (end.y - start.y)).timesPowerOfTwo(-1),
        (start.y + end.y - bulge * (end.x - start.x)).timesPowerOfTwo(-1),
    };
}

double magnitudeOf(const Curve& curve) {
    return std::visit(
        Overloaded{
            [](const Segment& segment) {
                return largestMagnitude(
                    {segment.start.x, segment.start.y, segment.end.x, segment.end.y});
            },
            [](const Arc& arc) {
                return largestMagnitude({arc.center.x, arc.center.y, arc.radius});
            },
            [](const Circle& circle) {
                return largestMagnitude({circle.center.x, circle.center.y, circle.radius});
            },
            [](const PolylineSegment& piece) {
                const double ends =
                    largestMagnitude({piece.start.x, piece.start.y, piece.end.x, piece.end.y});
                if (!isCurved(piece)) {
                    return ends;
                }
                const auto m = arcMidpoint(piece);
                return std::max(ends, largestMagnitude({m.x.toDouble(), m.y.toDouble()}));
            },
        },
        curve);
}

double magnitudeOf(std::span<const CurveMeaning> curves) noexcept {
    double result = 0.0;
    for (const auto& curve : curves) {
        result = std::max(result, curve.magnitude);
    }
    return result;
}

ArcRule arcRule(double sweep, Point2 d0, Point2 d1) {
    const Turn turn = sweep > 0.0 ? Turn::counterclockwise : Turn::clockwise;
    const auto start = exactVector(d0);
    const auto end = exactVector(d1);
    // alpha, the angle from D0 to D1 in the direction of the sweep, known by
    // its sector: enough for every comparison the rules make.
    const auto alpha = sectorOf(oriented(turn, crossSign(start, end)), dotSign(start, end));
    const double size = std::fabs(sweep);
    // alpha <= pi / 2 and alpha >= 3 pi / 2, read from the sectors.
    const bool upToQuarter =
        alpha == TurnSector::zero || alpha == TurnSector::first || alpha == TurnSector::quarter;
    const bool fromThreeQuarters =
        alpha == TurnSector::threeQuarters || alpha == TurnSector::fourth;
    if (size >= kFullTurn || (size >= kThreeQuarterTurn && upToQuarter)) {
        return ArcRule{turn, ArcSpan::whole, TurnSector::zero};
    }
    if ((size <= kQuarterTurn && fromThreeQuarters) || alpha == TurnSector::zero) {
        return ArcRule{turn, ArcSpan::point, TurnSector::zero};
    }
    return ArcRule{turn, ArcSpan::partial, alpha};
}

CurveMeaning meaningOf(const Curve& curve) {
    assert(!checkData(curve, 0).has_value());
    return std::visit(Overloaded{
                          [&curve](const Segment& segment) {
                              return lineMeaning(curve, segment.start, segment.end);
                          },
                          [](const Arc& arc) { return arcMeaning(arc); },
                          [](const Circle& circle) { return circleMeaning(circle); },
                          [](const PolylineSegment& piece) { return pieceMeaning(piece); },
                      },
                      curve);
}

Result<CurveMeaning> checkedMeaning(const Curve& curve, std::size_t input, Degenerate degenerate) {
    if (auto problem = checkData(curve, input)) {
        return std::move(*problem);
    }
    auto meaning = meaningOf(curve);
    if (meaning.degenerate && degenerate == Degenerate::reject) {
        return degenerateDiagnosis(input);
    }
    return meaning;
}

TurnSector sectorFromStart(const ArcMeaning& arc, const ExactVector& direction) {
    assert(arc.span != ArcSpan::point);
    return sectorOf(oriented(arc.turn, crossSign(arc.startDirection, direction)),
                    dotSign(arc.startDirection, direction));
}

Location locateOnArc(const ArcMeaning& arc, TurnSector fromStart,
                     Sign orientedCrossToEnd) noexcept {
    assert(arc.span != ArcSpan::point);
    switch (arc.span) {
        case ArcSpan::circle:
            return Location::inside;
        case ArcSpan::whole:
            return fromStart == TurnSector::zero ? Location::atStart : Location::inside;
        case ArcSpan::partial:
        case ArcSpan::point:
            break;
    }
    if (fromStart == TurnSector::zero) {
        return Location::atStart;
    }
    switch (compareTurns(fromStart, arc.endSector, orientedCrossToEnd)) {
        case Sign::negative:
            return Location::inside;
        case Sign::zero:
            return Location::atEnd;
        case Sign::positive:
            break;
    }
    return Location::afterEnd;
}

Location locateDirection(const ArcMeaning& arc, const ExactVector& direction) {
    const auto fromStart = sectorFromStart(arc, direction);
    const auto toEnd = oriented(arc.turn, crossSign(direction, arc.endDirection));
    return locateOnArc(arc, fromStart, toEnd);
}

Sign compareAlongArc(const ArcMeaning& arc, const ExactVector& u, const ExactVector& v) {
    return compareTurns(sectorFromStart(arc, u), sectorFromStart(arc, v),
                        oriented(arc.turn, crossSign(u, v)));
}

}  // namespace solid_space::geom2d::detail
