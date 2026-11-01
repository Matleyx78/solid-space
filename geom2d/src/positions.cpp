// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Positions of exact points along the curves (positions.hpp; stage 0.2,
// package P10).
//
// Notation. An exact point is P = (r + q sqrt(d)) / w (carriers.hpp,
// ExactPoint), with w > 0. A straight curve runs from S to E with direction
// U = E - S; a circular one has the homogeneous centre c / W, the squared
// radius rho / W^2 and the directions D0 and D1 of its ends from the centre.
// Every sign below is the sign of an exact number a + b sqrt(d), or of two
// radicals together (exact/quadratic.hpp), after multiplying by positive
// weights.
//
// Error budget of the stations (curves.hpp, kRelativeStationError = 2^-46 =
// 128 u, with u = 2^-53 the unit roundoff):
// - straight curve: the signed distance dot(P - S, U) / |U| correctly rounded,
//   at most u |s| (subnormal results: 2^-1075, below 2^-46 M since M >= |U| /
//   (2 sqrt 2) > 0);
// - circular curve: s = R * phi. R is correctly rounded (u). phi = base + the
//   angle within a quarter, where the angle comes from the arcsine series of a
//   correctly rounded sine or cosine x <= sqrt(2) / 2 (relative error u,
//   amplified by at most sqrt(2) through the arcsine); the series is cut where
//   the rest is below 2^-57 asin(x); Horner's form with positive terms and
//   x^2 <= 1/2 rounds each step within u and damps the errors of the higher
//   terms, so the series is within 4 u of asin(x). Adding the base, taking pi/2
//   minus the arcsine, and the doubles of pi/4, pi/2, pi, 3 pi/2 and 2 pi add a
//   few u of the result, which is at least pi/4 there; the clamps to the
//   bounds of an eighth of a turn move phi only towards the exact value. The
//   product with R adds u. In all, below 16 u of max(M, |s|), far below 128 u.

#include "positions.hpp"

#include "exact/quadratic.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <variant>

namespace solid_space::geom2d::detail {
namespace {

using exact::QuadraticNumber;
using robust::Sign;

// Bits of the square root that gives the first approximation of a quotient by
// sqrt(k) (as in exact/quadratic.cpp): a relative error below 2^-127, so that
// the approximation is within about one ulp and the exact steps below rarely
// move it.
constexpr std::int64_t kRootBits = 128;

// pi / 4 and pi, the doubles nearest to the exact values (the division by 4
// is exact), completing the angle
// constants of curves.hpp.
constexpr double kEighthTurn = std::numbers::pi / 4.0;
constexpr double kHalfTurn = std::numbers::pi;

// The angle at the start of each quarter turn, and 2 pi at the end of the last.
constexpr std::array<double, 5> kQuarterStart{0.0, kQuarterTurn, kHalfTurn, kThreeQuarterTurn,
                                              kFullTurn};

// Terms of the arcsine series asin(x) = sum c_k x^(2k + 1) used by the
// stations, for 0 <= x <= sqrt(2) / 2: c_k <= 1 / ((2k + 1) sqrt(pi k)) and
// x^(2k) <= 2^-k, so the term k = 48 is below 2^-58 x and the rest of the
// series, a geometric tail of ratio at most 1/2, below 2^-57 x <= 2^-57 asin(x).
constexpr std::size_t kArcsineTerms = 48;

// c_0 = 1, c_k = c_(k-1) (2k - 1)^2 / (2k (2k + 1)): the exact coefficients
// (2k)! / (4^k (k!)^2 (2k + 1)) up to a rounding of a few ulp each, which only
// changes the result within the error budget above. All positive, which is
// what makes the series non-decreasing (positions.hpp).
constexpr std::array<double, kArcsineTerms> arcsineCoefficients() {
    std::array<double, kArcsineTerms> coefficients{};
    double term = 1.0;
    coefficients.at(0) = term;
    for (std::size_t k = 1; k < kArcsineTerms; ++k) {
        const auto odd = static_cast<double>((2 * k) - 1);
        const auto even = static_cast<double>(2 * k);
        term *= (odd * odd) / (even * (even + 1.0));
        coefficients.at(k) = term;
    }
    return coefficients;
}

constexpr std::array<double, kArcsineTerms> kArcsine = arcsineCoefficients();

// asin(x) for 0 <= x <= sqrt(2) / 2 (see kArcsineTerms), in Horner's form on
// x^2. Every step is a product or a sum of non-negative doubles, each rounded
// to nearest, so the result never decreases when x grows.
double arcsine(double x) {
    assert(x >= 0.0 && x <= 0.71 && "arcsine is evaluated within an eighth of a turn");
    const double square = x * x;
    double sum = kArcsine.back();
    for (auto coefficient = kArcsine.rbegin() + 1; coefficient != kArcsine.rend(); ++coefficient) {
        sum = (sum * square) + *coefficient;
    }
    return x * sum;
}

Dyadic dot(const ExactVector& a, const ExactVector& b) {
    return a.x * b.x + a.y * b.y;
}

Dyadic cross(const ExactVector& a, const ExactVector& b) {
    return a.x * b.y - a.y * b.x;
}

ExactVector minus(const ExactVector& a, const ExactVector& b) {
    return {a.x - b.x, a.y - b.y};
}

ExactVector times(const ExactVector& vector, const Dyadic& factor) {
    return {vector.x * factor, vector.y * factor};
}

// The vector turned by a quarter in the direction of travel.
ExactVector quarterTurn(const ExactVector& vector, Turn turn) {
    if (turn == Turn::counterclockwise) {
        return {-vector.y, vector.x};
    }
    return {vector.y, -vector.x};
}

Sign negated(Sign sign) noexcept {
    return static_cast<Sign>(-static_cast<int>(sign));
}

int sectorIndex(TurnSector sector) noexcept {
    return static_cast<int>(sector);
}

bool isOpenQuarter(TurnSector sector) noexcept {
    return sectorIndex(sector) % 2 == 1;
}

// The ties-to-even choice between two neighbouring doubles.
double evenOf(double a, double b) noexcept {
    return (std::bit_cast<std::uint64_t>(a) & 1U) == 0 ? a : b;
}

// The sign of (a + b sqrt(d)) / (w sqrt(k)) - m, with w > 0 and k > 0: the
// sign of a + b sqrt(d) - m w sqrt(k).
Sign signAgainst(const Dyadic& a, const Dyadic& b, const Dyadic& d, const Dyadic& w,
                 const Dyadic& k, bool unitK, const Dyadic& m) {
    if (unitK) {
        return exact::signOf(a - m * w, b, d);
    }
    return exact::signOf(QuadraticNumber{a, b, d}, QuadraticNumber{-(m * w), Dyadic(), Dyadic()},
                         k);
}

// +0 for an exact zero, whatever sign the arithmetic gave it.
double canonicalZero(double value) noexcept {
    return value == 0.0 ? 0.0 : value;
}

// The exact value (a + b sqrt(d)) / (w sqrt(k)) of correctlyRounded().
struct Quotient {
    const Dyadic& a;
    const Dyadic& b;
    const Dyadic& d;
    const Dyadic& w;
    const Dyadic& k;
    bool unitK;
};

// Steps of one ulp allowed from the rough start before giving it up.
constexpr int kRoughSteps = 4;

// Values beyond this magnitude are not rounded here (positions.hpp,
// correctlyRounded): their neighbours would leave the doubles.
constexpr double kLargestRounded = 0x1p1000;

// A rough double for the quotient, from the doubles nearest to its parts: a
// few ulp from the exact value when the parts are within the range of the
// doubles, since a + b sqrt(d) is never computed with cancellation (the
// conjugate (a^2 - b^2 d) / (a - b sqrt(d)) is used when the two terms have
// opposite signs, with its numerator exact). Outside that range it may be far
// off, or not finite: nearestFrom() then gives up on it. std::sqrt is
// correctly rounded (IEEE 754), but nothing here needs that: the result of
// correctlyRounded() does not depend on these bits.
double roughValue(const Quotient& value) {
    const double a = value.a.toDouble();
    const double b = value.b.toDouble();
    const double d = value.d.toDouble();
    const double divisor = value.w.toDouble() * std::sqrt(value.k.toDouble());
    if (value.b.isZero() || value.d.isZero()) {
        return a / divisor;
    }
    const double root = b * std::sqrt(d);
    if (value.a.isZero() || value.a.sign() == value.b.sign()) {
        return (a + root) / divisor;
    }
    const Dyadic numerator = value.a * value.a - value.b * value.b * value.d;
    return numerator.toDouble() / ((a - root) * divisor);
}

// The sign of the quotient minus (low + high) / 2.
Sign againstMiddle(const Quotient& value, double low, double high) {
    const Dyadic middle = (Dyadic(low) + Dyadic(high)).timesPowerOfTwo(-1);
    return signAgainst(value.a, value.b, value.d, value.w, value.k, value.unitK, middle);
}

// The correctly rounded quotient, found from `start` by exact comparisons with
// the midpoints between neighbouring doubles: up while the quotient is above
// the next midpoint, otherwise down while it is below the previous one; an
// exact midpoint goes to the even neighbour. Nothing when the start is not a
// usable double or the quotient is more than `steps` ulp away.
std::optional<double> nearestFrom(const Quotient& value, double start, int steps) {
    constexpr double kInfinity = std::numeric_limits<double>::infinity();
    if (!std::isfinite(start) || std::fabs(start) > kLargestRounded) {
        return std::nullopt;
    }
    double result = start;
    for (int moved = 0;; ++moved) {
        const double up = std::nextafter(result, kInfinity);
        const Sign side = againstMiddle(value, result, up);
        if (side == Sign::zero) {
            return evenOf(result, up);
        }
        if (side == Sign::negative) {
            if (moved > 0) {
                return result;
            }
            break;
        }
        if (moved == steps || std::fabs(up) > kLargestRounded) {
            return std::nullopt;
        }
        result = up;
    }
    for (int moved = 0;; ++moved) {
        const double down = std::nextafter(result, -kInfinity);
        const Sign side = againstMiddle(value, down, result);
        if (side == Sign::zero) {
            return evenOf(down, result);
        }
        if (side == Sign::positive) {
            return result;
        }
        if (moved == steps || std::fabs(down) > kLargestRounded) {
            return std::nullopt;
        }
        result = down;
    }
}

// --- Straight curves -------------------------------------------------------

// dot(P - X, U) * w = (dot(U, r) - w dot(U, X)) + dot(U, q) sqrt(d), for a
// point X of doubles: the position of P along the line, from X, times
// w |U|^2 > 0.
QuadraticNumber alongFrom(const ExactVector& direction, Point2 origin, const ExactPoint& point) {
    return QuadraticNumber{
        dot(direction, point.rational) - point.weight * dot(direction, exactVector(origin)),
        dot(direction, point.radical), point.radicand};
}

ExactVector directionOf(const LineMeaning& line) {
    return minus(exactVector(line.end), exactVector(line.start));
}

Location locateOnLine(const LineMeaning& line, const ExactPoint& point) {
    const auto direction = directionOf(line);
    switch (exact::signOf(alongFrom(direction, line.start, point))) {
        case Sign::negative:
            return Location::beforeStart;
        case Sign::zero:
            return Location::atStart;
        case Sign::positive:
            break;
    }
    switch (exact::signOf(alongFrom(direction, line.end, point))) {
        case Sign::negative:
            return Location::inside;
        case Sign::zero:
            return Location::atEnd;
        case Sign::positive:
            break;
    }
    return Location::afterEnd;
}

// dot(A, U) - dot(B, U), times wA wB > 0, with two radicands: as
// compareCoordinate of carriers.cpp, along U.
Sign compareOnLine(const LineMeaning& line, const ExactPoint& a, const ExactPoint& b) {
    const auto direction = directionOf(line);
    const QuadraticNumber x{
        dot(direction, a.rational) * b.weight - dot(direction, b.rational) * a.weight,
        dot(direction, a.radical) * b.weight, a.radicand};
    const QuadraticNumber y{-(dot(direction, b.radical) * a.weight), Dyadic(), Dyadic()};
    return exact::signOf(x, y, b.radicand);
}

// dot(P - S, U) / |U|, correctly rounded.
double stationOnLine(const LineMeaning& line, const ExactPoint& point) {
    const auto direction = directionOf(line);
    const auto along = alongFrom(direction, line.start, point);
    return correctlyRounded(along.a, along.b, along.d, point.weight, dot(direction, direction));
}

// --- Circular curves -------------------------------------------------------

// The direction of P from the centre, P - c / W, times w W > 0:
// (W r - w c) + W q sqrt(d), kept as its rational and radical vectors.
struct Direction {
    ExactVector rational;
    ExactVector radical;
};

Direction directionFromCentre(const CircleSupport& circle, const ExactPoint& point) {
    const ExactVector centre{circle.centerX, circle.centerY};
    return Direction{minus(times(point.rational, circle.weight), times(centre, point.weight)),
                     times(point.radical, circle.weight)};
}

// The sign of cross(g, V) and dot(g, V) for a dyadic vector g.
Sign crossWith(const ExactVector& g, const Direction& v, const Dyadic& radicand) {
    return exact::signOf(cross(g, v.rational), cross(g, v.radical), radicand);
}

Sign dotWith(const ExactVector& g, const Direction& v, const Dyadic& radicand) {
    return exact::signOf(dot(g, v.rational), dot(g, v.radical), radicand);
}

TurnSector sectorFrom(const ArcMeaning& arc, const Direction& v, const Dyadic& radicand) {
    return sectorOf(oriented(arc.turn, crossWith(arc.startDirection, v, radicand)),
                    dotWith(arc.startDirection, v, radicand));
}

Location locateOnCircle(const ArcMeaning& arc, const ExactPoint& point) {
    assert(arc.span != ArcSpan::point);
    if (arc.span == ArcSpan::circle) {
        return Location::inside;
    }
    const auto v = directionFromCentre(arc.circle, point);
    const auto fromStart = sectorFrom(arc, v, point.radicand);
    // The sign towards D1 is read only in the quarter of D1 (curve_meaning.hpp,
    // locateOnArc and compareTurns): computed only there.
    Sign toEnd = Sign::zero;
    if (arc.span == ArcSpan::partial && fromStart == arc.endSector && isOpenQuarter(fromStart)) {
        // cross(v, D1) = -cross(D1, v).
        toEnd = oriented(arc.turn, negated(crossWith(arc.endDirection, v, point.radicand)));
    }
    return locateOnArc(arc, fromStart, toEnd);
}

// cross(Va, Vb) with Va = A + B sqrt(da) and Vb = C + E sqrt(db):
//     cross(A, C) + cross(B, C) sqrt(da) + (cross(A, E) + cross(B, E) sqrt(da)) sqrt(db),
// the form x + y sqrt(e) of exact::signOf, with x and y sharing sqrt(da).
Sign crossOfDirections(const Direction& a, const Dyadic& radicandA, const Direction& b,
                       const Dyadic& radicandB) {
    const QuadraticNumber x{cross(a.rational, b.rational), cross(a.radical, b.rational), radicandA};
    const QuadraticNumber y{cross(a.rational, b.radical), cross(a.radical, b.radical), radicandA};
    return exact::signOf(x, y, radicandB);
}

Sign compareOnCircle(const ArcMeaning& arc, const ExactPoint& a, const ExactPoint& b) {
    const auto va = directionFromCentre(arc.circle, a);
    const auto vb = directionFromCentre(arc.circle, b);
    const auto sa = sectorFrom(arc, va, a.radicand);
    const auto sb = sectorFrom(arc, vb, b.radicand);
    Sign orientedCross = Sign::zero;
    if (sa == sb && isOpenQuarter(sa)) {
        orientedCross = oriented(arc.turn, crossOfDirections(va, a.radicand, vb, b.radicand));
    }
    return compareTurns(sa, sb, orientedCross);
}

// The angle phi in [0, 2 pi) from D0 to the direction of P, in the direction
// of travel, rounded by a non-decreasing function of the exact angle (see
// positions.hpp and the error budget at the top).
//
// The quarter q of the angle comes from its sector. Within it, with r the axis
// D0 turned by q quarters and n the next axis, both of length |D0|, the point
// has the components x = dot(r, v) > 0 and y = dot(n, v) >= 0 (or x = 0 and
// y > 0 only at the next axis, which is the next quarter). Below the diagonal
// (y < x) the angle within the quarter is asin(y / (|D0| R)), which grows with
// it; on and above it, pi/2 - asin(x / (|D0| R)), since x / (|D0| R) falls.
// With v = V / (w W) and R = sqrt(rho) / W,
//     g . v / (|D0| R) = (g . A + (g . B) sqrt(d)) / (w sqrt(rho |D0|^2)).
double turnAngle(const ArcMeaning& arc, const ExactPoint& point) {
    const auto v = directionFromCentre(arc.circle, point);
    const auto sector = sectorFrom(arc, v, point.radicand);
    const auto quarter = static_cast<std::size_t>(sectorIndex(sector) / 2);
    ExactVector axis = arc.startDirection;
    for (std::size_t i = 0; i < quarter; ++i) {
        axis = quarterTurn(axis, arc.turn);
    }
    const ExactVector next = quarterTurn(axis, arc.turn);
    const Dyadic k = arc.circle.radiusSquared * dot(arc.startDirection, arc.startDirection);
    const double start = kQuarterStart.at(quarter);
    const double end = kQuarterStart.at(quarter + 1);
    const double middle = start + kEighthTurn;
    const auto diagonal = minus(next, axis);
    if (dotWith(diagonal, v, point.radicand) == Sign::negative) {
        const double sine = correctlyRounded(dot(next, v.rational), dot(next, v.radical),
                                             point.radicand, point.weight, k);
        return std::min(start + arcsine(sine), middle);
    }
    const double cosine = correctlyRounded(dot(axis, v.rational), dot(axis, v.radical),
                                           point.radicand, point.weight, k);
    return std::max(std::min(start + (kQuarterTurn - arcsine(cosine)), end), middle);
}

double stationOnCircle(const ArcMeaning& arc, const ExactPoint& point) {
    // R = sqrt(rho) / W: the radius itself for a Circle and an Arc.
    const double radius = correctlyRounded(Dyadic(), Dyadic(1.0), arc.circle.radiusSquared,
                                           arc.circle.weight, Dyadic(1.0));
    return radius * turnAngle(arc, point);
}

}  // namespace

bool takesPart(Location location, Extension extension) noexcept {
    if (extension == Extension::extended) {
        return true;
    }
    return location == Location::atStart || location == Location::inside ||
           location == Location::atEnd;
}

Location locate(const CurveMeaning& curve, const ExactPoint& point) {
    assert(!curve.degenerate);
    if (const auto* line = std::get_if<LineMeaning>(&curve.support)) {
        return locateOnLine(*line, point);
    }
    return locateOnCircle(std::get<ArcMeaning>(curve.support), point);
}

Sign compareAlong(const CurveMeaning& curve, const ExactPoint& a, const ExactPoint& b) {
    assert(!curve.degenerate);
    if (const auto* line = std::get_if<LineMeaning>(&curve.support)) {
        return compareOnLine(*line, a, b);
    }
    return compareOnCircle(std::get<ArcMeaning>(curve.support), a, b);
}

double station(const CurveMeaning& curve, const ExactPoint& point) {
    assert(!curve.degenerate);
    if (const auto* line = std::get_if<LineMeaning>(&curve.support)) {
        return stationOnLine(*line, point);
    }
    return stationOnCircle(std::get<ArcMeaning>(curve.support), point);
}

CurvePosition positionOn(const CurveMeaning& curve, const ExactPoint& point) {
    return CurvePosition{locate(curve, point), station(curve, point)};
}

double correctlyRounded(const Dyadic& a, const Dyadic& b, const Dyadic& d, const Dyadic& w,
                        const Dyadic& k) {
    assert(d.sign() != Sign::negative && w.sign() == Sign::positive && k.sign() == Sign::positive);
    const Quotient value{a, b, d, w, k, compare(k, Dyadic(1.0)) == Sign::zero};
    // The exact steps decide the result: the starting point only changes how
    // many of them are needed. A rough start that is far off is abandoned for
    // the accurate one of exact/quadratic.hpp.
    if (const auto rough = nearestFrom(value, roughValue(value), kRoughSteps)) {
        return canonicalZero(*rough);
    }
    const double accurate = value.unitK
                                ? exact::quadraticToDouble(a, b, d, w)
                                : exact::quadraticToDouble(a, b, d, w * floorSqrt(k, kRootBits));
    const auto result = nearestFrom(value, accurate, std::numeric_limits<int>::max());
    assert(result.has_value());
    return canonicalZero(result.value_or(accurate));
}

Point2 nearestPoint(const ExactPoint& point) {
    const Dyadic one(1.0);
    return Point2{
        correctlyRounded(point.rational.x, point.radical.x, point.radicand, point.weight, one),
        correctlyRounded(point.rational.y, point.radical.y, point.radicand, point.weight, one)};
}

ExactPoint pointInDirection(const CircleSupport& circle, const ExactVector& direction) {
    const Dyadic norm = dot(direction, direction);
    assert(!norm.isZero() && "a direction must not be the zero vector");
    return ExactPoint{times(ExactVector{circle.centerX, circle.centerY}, norm), direction,
                      circle.radiusSquared * norm, circle.weight * norm};
}

ExactPoint startPoint(const CurveMeaning& curve) {
    assert(!curve.degenerate);
    if (const auto* line = std::get_if<LineMeaning>(&curve.support)) {
        return exactPoint(line->start);
    }
    const auto& arc = std::get<ArcMeaning>(curve.support);
    return pointInDirection(arc.circle, arc.startDirection);
}

ExactPoint endPoint(const CurveMeaning& curve) {
    assert(!curve.degenerate);
    if (const auto* line = std::get_if<LineMeaning>(&curve.support)) {
        return exactPoint(line->end);
    }
    const auto& arc = std::get<ArcMeaning>(curve.support);
    return pointInDirection(arc.circle, arc.endDirection);
}

}  // namespace solid_space::geom2d::detail
