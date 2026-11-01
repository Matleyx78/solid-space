// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// tangentsFrom and tangentCircles of tangency.hpp (stage 0.2, package P15;
// ADR 0011), and the positions of points with nested roots
// (tangency_internal.hpp).
//
// Tangents from a point. With the circle of the curve written as in
// curve_meaning.hpp (centre c / W, squared radius rho / W^2, W > 0) and the
// point Q of doubles, let q = W Q - c, so that |Q - C|^2 = |q|^2 / W^2. Q is
// outside, on or inside the circle as |q|^2 - rho is positive, zero or
// negative. When it is outside, the two points of tangency are
//     T = (c |q|^2 + rho q +- sqrt(e) perp(q)) / (W |q|^2),
//     e = rho (|q|^2 - rho) > 0,   perp(q) = (-q.y, q.x):
// T - C is rho q / (W |q|^2) along q and sqrt(e) / (W |q|^2) across it, so
// |T - C|^2 = (rho^2 + e) / (W^2 |q|^2) = rho / W^2 and (T - C) . (T - Q) = 0.
// Both are exact points of carriers.hpp (one radicand), so their positions come
// from positions.hpp and they are correctly rounded (nearestPoint). Every
// decision is exact; the placement of Q has a floating-point filter.
//
// Floating-point filter of the placement (ADR 0011, point 10: the tangent
// snap asks for it at every movement of the mouse). For an Arc or a Circle the
// centre and the radius are doubles. With u = 2^-53 the unit roundoff, the
// computed dx = Q.x - C.x is dx (1 + e) with |e| <= u (exact when subnormal),
// each square adds a relative error u and an absolute one of at most 2^-1075
// (underflow), and the two sums and the final difference add u of the sum of
// the magnitudes. So the computed |q|^2 - R^2 differs from the exact one by at
// most 6 u (dx^2 + dy^2 + R^2) (1 + O(u)) + 3 * 2^-1075. The filter decides
// only beyond 16 u of the computed sum plus 2^-1069, far above both terms;
// otherwise the exact sign decides. A curved PolylineSegment goes to the exact
// sign at once.
//
// Tangent circles. The centre of a circle of radius r tangent to a curve lies
// on a parallel of the curve at distance r: a line moved by +-r along its
// normal (offset_carriers.hpp, offsetCarrier), or a circle of radius R + r or
// |R - r| around the same centre. With the radius of the curve
// sqrt(rho) / W, the radius of a parallel circle is |n| / W with
//     n = sqrt(rho) + k r W,   k = +1 (outside) or -1 (inside),
// and its squared radius n^2 / W^2. n < 0 (k = -1, r > R) is the circle of the
// tangent circles that enclose the curve. n == 0 (r == R) leaves the centre
// alone, and the only circle of radius r around it is the circle of the curve
// itself: it touches the curve everywhere, not at a point of tangency, so it is
// not a tangent circle and that parallel is dropped. The centres are the
// meetings of a parallel of each curve (meetOffset): at most two for each of
// the four pairs, eight in all, all different (the two parallels of a curve
// are disjoint). Two parallels that are the same line or circle give
// infinitely many centres: the two curves on the same line or circle, two
// parallel lines at distance 2 r, two concentric circles whose radii differ or
// add up to 2 r (tangency.hpp, TangentCircles).
//
// Points of tangency, from a centre P = X / w:
// - on a line through S with direction U, the foot of the perpendicular,
//       T = S + U (P - S) . U / |U|^2
//         = (S w |U|^2 + U (X . U - w S . U)) / (w |U|^2);
// - on a circle, the point of the circle in the direction of P from the centre
//   (away from it when n < 0): T = C + (P - C) sqrt(rho) / n, because
//   |P - C| = |n| / W. With a dyadic sqrt(rho) the factor is a quotient of
//   dyadic numbers; otherwise, with a = k r W,
//       sqrt(rho) / (sqrt(rho) + a) = (rho - a sqrt(rho)) / (rho - a^2),
//   whose denominator is not zero (sqrt(rho) is not dyadic). So
//       T = (c w d + (X W - c w) f) / (w W d)
//   for the factor f / d, with d > 0.
// The side of a circle on a line is the side of its centre, the sign of k
// (offsetCarrier moves positive distances to the left); on a circular curve it
// is inside (k = -1, n > 0), left for a counterclockwise curve, or outside,
// right for it (tangency.hpp, TangentCircle).
//
// Towers. Each of the four pairs of parallels is computed in a tower of its
// own: one root for the normal of each straight curve or the radius of each
// curved PolylineSegment (Arc and Circle have a dyadic radius), and one for
// the discriminant of the pair, at most three. A single tower for the four
// pairs would hold six, and an exact sign at that depth costs up to 3^6 signs
// of products (exact/tower.hpp). Points of different pairs are compared only to
// order the circles: by their stations and correctly rounded coordinates,
// which never decrease along the exact order and are functions of the exact
// point, so that different values decide; equal values (the same point of
// tangency, most often) are compared exactly in a tower that holds the roots of
// both pairs (Lift), at most four. A full tower cannot happen; it would still
// give a diagnosis, never a silent fallback.
//
// Centres beyond the domain. Two almost parallel lines meet far away, and the
// centres of their tangent circles with them, possibly beyond the doubles.
// tangency.hpp lists no count of omitted circles (unlike
// PairIntersections::omittedBeyondDomain), and a circle is never left out
// silently: a centre beyond robust::kMaxCoordinate gives the diagnosis
// outOfDomain about the radius (input 2), as offsetCurve() does for a point of
// its result (offset.hpp).
//
// Positions of points of the tower (tangency_internal.hpp). The same exact
// tests as positions.cpp, written for numbers of the tower: the sign of
// (P - S) . U along a line; the sectors of curve_meaning.hpp from D0 for a
// circular curve. The stations follow the rule of positions.hpp, so that they
// are the same function of the exact point: the signed distance along a line,
// and for a circular curve the radius times the angle, made of the arcsine
// series of the correctly rounded sine or cosine within an eighth of a turn.
// The constants and the series below are those of positions.cpp, and must
// stay the same (tests/geom2d/tangency_test.cpp compares the two). A value
// n / (w sqrt(k)) is correctly rounded by comparisons with the midpoints m
// between neighbouring doubles, filtered in double-double arithmetic and
// otherwise exact, from n^2 and m^2 w^2 k: no root of k is adjoined. The error
// budget is that of positions.cpp.

#include "solid_space/geom2d/tangency.hpp"

#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "exact/dyadic.hpp"
#include "exact/tower.hpp"
#include "offset_carriers.hpp"
#include "positions.hpp"
#include "solid_space/robust/point.hpp"
#include "solid_space/robust/predicates.hpp"
#include "tangency_internal.hpp"

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
#include <utility>
#include <variant>
#include <vector>

namespace solid_space::geom2d {
namespace detail {
namespace {

// --- Positions of points of the tower ----------------------------------------

// As in positions.cpp: the bits of the first approximation of sqrt(k), and the
// angle constants that complete those of curves.hpp.
constexpr std::int64_t kRootBits = 128;
constexpr double kEighthTurn = std::numbers::pi / 4.0;
constexpr double kHalfTurn = std::numbers::pi;
constexpr std::array<double, 5> kQuarterStart{0.0, kQuarterTurn, kHalfTurn, kThreeQuarterTurn,
                                              kFullTurn};

// The arcsine series of positions.cpp, term for term (see the error budget
// there): 48 terms, c_0 = 1, c_k = c_(k-1) (2k - 1)^2 / (2k (2k + 1)).
constexpr std::size_t kArcsineTerms = 48;

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

// asin(x) for 0 <= x <= sqrt(2) / 2, in Horner's form on x^2, as in
// positions.cpp: non-decreasing in x.
double arcsine(double x) {
    assert(x >= 0.0 && x <= 0.71 && "arcsine is evaluated within an eighth of a turn");
    const double square = x * x;
    double sum = kArcsine.back();
    for (auto coefficient = kArcsine.rbegin() + 1; coefficient != kArcsine.rend(); ++coefficient) {
        sum = (sum * square) + *coefficient;
    }
    return x * sum;
}

Sign negated(Sign sign) noexcept {
    return static_cast<Sign>(-static_cast<int>(sign));
}

Dyadic dot(const ExactVector& a, const ExactVector& b) {
    return a.x * b.x + a.y * b.y;
}

ExactVector minus(const ExactVector& a, const ExactVector& b) {
    return {a.x - b.x, a.y - b.y};
}

// The vector turned by a quarter in the direction of travel.
ExactVector quarterTurn(const ExactVector& vector, Turn turn) {
    if (turn == Turn::counterclockwise) {
        return {-vector.y, vector.x};
    }
    return {vector.y, -vector.x};
}

int sectorIndex(TurnSector sector) noexcept {
    return static_cast<int>(sector);
}

bool isOpenQuarter(TurnSector sector) noexcept {
    return sectorIndex(sector) % 2 == 1;
}

// --- Floating-point filter ---------------------------------------------------
//
// Every sign of a number of a tower is first read from an approximation in
// double-double arithmetic (a value hi + lo with about 106 bits) and a bound of
// its distance from the exact value; the exact sign of the tower decides only
// when the bound does not. With about 100 good bits the bound also decides
// the comparisons with the midpoints between doubles that correct rounding
// needs (half an ulp, 2^-54 relative), unless the value is a midpoint itself or
// its terms cancel. The answers are those of the exact signs whenever the
// filter decides, so nothing depends on it but the time.
//
// The operations are the classical error-free ones: TwoSum (Knuth) and
// TwoProduct with Dekker's split, exact with round to nearest and no
// contraction into fused operations (ADR 0008, point 5), as long as nothing
// overflows (all magnitudes are kept below 2^900) and nothing underflows. Each
// bound below adds, to the errors carried by the operands, the roundings that
// the operation leaves (each at most u = 2^-53 of its result) and an absolute
// term for underflow; the bound itself is computed in double, so it is
// multiplied by 1 + 2^-49 (16 u) to cover its own roundings. A bound that is
// not finite means "unknown".

constexpr double kUnitRoundoff = 0x1p-53;
constexpr double kInflate = 1.0 + 0x1p-49;
// Below about 2^-969 the error-free transformations are no longer exact; the
// filter leaves such values to the exact signs.
constexpr double kTiny = 0x1p-960;
// Above this magnitude the split of Dekker could overflow.
constexpr double kLargestFiltered = 0x1p900;
// 2^27 + 1: Dekker's split of a double into two halves of 26 bits.
constexpr double kSplitter = 134217729.0;
constexpr double kInfinity = std::numeric_limits<double>::infinity();

// a + b = sum + error exactly (Knuth's TwoSum).
struct Exact2 {
    double high = 0.0;
    double low = 0.0;
};

Exact2 twoSum(double a, double b) {
    const double sum = a + b;
    const double virtualB = sum - a;
    const double virtualA = sum - virtualB;
    return {sum, (a - virtualA) + (b - virtualB)};
}

Exact2 split(double a) {
    const double scaled = kSplitter * a;
    const double high = scaled - (scaled - a);
    return {high, a - high};
}

// a b = product + error exactly (Dekker's TwoProduct).
Exact2 twoProduct(double a, double b) {
    const double product = a * b;
    const auto [aHigh, aLow] = split(a);
    const auto [bHigh, bLow] = split(b);
    const double error =
        (((aHigh * bHigh) - product) + (aHigh * bLow) + (aLow * bHigh)) + (aLow * bLow);
    return {product, error};
}

// An approximation high + low of an exact value, within `error` of it.
struct Wide {
    double high = 0.0;
    double low = 0.0;
    double error = 0.0;
};

Wide unknown() {
    return {0.0, 0.0, kInfinity};
}

double magnitudeOf(const Wide& x) {
    return std::fabs(x.high) + std::fabs(x.low);
}

Wide checked(Wide x) {
    if (!std::isfinite(x.high) || !std::isfinite(x.error) || std::fabs(x.high) > kLargestFiltered) {
        return unknown();
    }
    return x;
}

// A dyadic number: its nearest double and the nearest double of the rest.
Wide wide(const Dyadic& number) {
    const double high = number.toDouble();
    if (!std::isfinite(high) || std::fabs(high) > kLargestFiltered) {
        return unknown();
    }
    const double low = (number - Dyadic(high)).toDouble();
    return {high, low, (kUnitRoundoff * std::fabs(low)) + kTiny};
}

Wide add(const Wide& a, const Wide& b) {
    const auto [sum, carry] = twoSum(a.high, b.high);
    const double rest = (a.low + b.low) + carry;
    const auto [high, low] = twoSum(sum, rest);
    const double error =
        a.error + b.error +
        (2.0 * kUnitRoundoff * (std::fabs(a.low) + std::fabs(b.low) + std::fabs(carry)));
    return checked({high, low, (error * kInflate) + kTiny});
}

Wide negate(const Wide& a) {
    return {-a.high, -a.low, a.error};
}

Wide subtract(const Wide& a, const Wide& b) {
    return add(a, negate(b));
}

Wide multiply(const Wide& a, const Wide& b) {
    const auto [product, carry] = twoProduct(a.high, b.high);
    const double cross1 = a.high * b.low;
    const double cross2 = a.low * b.high;
    const double rest = (cross1 + cross2) + carry;
    const auto [high, low] = twoSum(product, rest);
    const double rounding =
        3.0 * kUnitRoundoff * (std::fabs(cross1) + std::fabs(cross2) + std::fabs(carry));
    const double error = (magnitudeOf(a) * b.error) + (magnitudeOf(b) * a.error) +
                         (a.error * b.error) + std::fabs(a.low * b.low) + rounding;
    return checked({high, low, (error * kInflate) + kTiny});
}

// sqrt(r) by one Newton step from the double root s: s + (r - s^2) / (2 s),
// with s^2 exact (TwoProduct; r.high - s^2 is exact by Sterbenz's lemma). The
// step leaves at most c^2 / s for the correction c; the propagated error is
// |sqrt(R) - sqrt(r)| <= e / sqrt(r - e).
Wide squareRootOf(const Wide& radicand) {
    const double margin = (radicand.high + radicand.low) - radicand.error;
    if (!(margin > kTiny) || !(radicand.high > 0.0)) {
        return unknown();
    }
    const double root = std::sqrt(radicand.high);
    const auto [square, carry] = twoProduct(root, root);
    const double difference = radicand.high - square;
    const double residual = (difference - carry) + radicand.low;
    const double correction = residual / (2.0 * root);
    const auto [high, low] = twoSum(root, correction);
    const double residualError =
        2.0 * kUnitRoundoff * (std::fabs(difference) + std::fabs(carry) + std::fabs(radicand.low));
    const double error = (radicand.error / std::sqrt(margin)) + ((correction * correction) / root) +
                         (residualError / root) + (2.0 * kUnitRoundoff * std::fabs(correction));
    return checked({high, low, (error * kInflate) + kTiny});
}

// The sign of the exact value, when the bound decides it.
std::optional<Sign> certainSign(const Wide& x) {
    const double value = x.high != 0.0 ? x.high : x.low;
    const double rest = x.high != 0.0 ? std::fabs(x.low) : 0.0;
    if (std::fabs(value) > x.error + rest) {
        return value > 0.0 ? Sign::positive : Sign::negative;
    }
    return std::nullopt;
}

// The numbers of a tower in double-double arithmetic, with the exact signs
// behind them.
class Filtered {
public:
    explicit Filtered(const Tower& tower) : m_tower(tower) {
        m_roots.reserve(tower.size());
        for (const auto& radicand : tower.radicands()) {
            m_roots.push_back(squareRootOf(evaluate(radicand)));
        }
    }

    [[nodiscard]] const Tower& tower() const noexcept { return m_tower; }

    // The value of x with a bound of its error.
    [[nodiscard]] Wide evaluate(const TowerNumber& x) const {
        const auto coefficients = x.coefficients();
        Wide sum;
        for (std::size_t mask = 0; mask < coefficients.size(); ++mask) {
            if (coefficients[mask].isZero()) {
                continue;
            }
            Wide term = wide(coefficients[mask]);
            for (std::size_t root = 0; (mask >> root) != 0; ++root) {
                if (((mask >> root) & 1U) != 0) {
                    term = multiply(term, m_roots[root]);
                }
            }
            sum = add(sum, term);
        }
        return sum;
    }

    [[nodiscard]] Sign sign(const TowerNumber& x) const {
        if (const auto certain = certainSign(evaluate(x))) {
            return *certain;
        }
        return m_tower.sign(x);
    }

    [[nodiscard]] Sign compare(const TowerNumber& a, const TowerNumber& b) const {
        return sign(a - b);
    }

    [[nodiscard]] TowerNumber product(const TowerNumber& a, const TowerNumber& b) const {
        return m_tower.product(a, b);
    }

private:
    const Tower& m_tower;
    std::vector<Wide> m_roots;
};

// --- Correct rounding --------------------------------------------------------

// Values beyond this magnitude are not rounded here: their neighbours would
// leave the doubles (as in positions.cpp).
constexpr double kLargestRounded = 0x1p1000;

// Steps of one ulp allowed from the filtered start before giving it up.
constexpr int kRoughSteps = 4;

// The ties-to-even choice between two neighbouring doubles.
double evenOf(double a, double b) noexcept {
    return (std::bit_cast<std::uint64_t>(a) & 1U) == 0 ? a : b;
}

// +0 for an exact zero, whatever sign the arithmetic gave it.
double canonicalZero(double value) noexcept {
    return value == 0.0 ? 0.0 : value;
}

Dyadic midpoint(double low, double high) {
    return (Dyadic(low) + Dyadic(high)).timesPowerOfTwo(-1);
}

// The correctly rounded value of an exact number (nearest, ties to even), found
// from `start` by comparisons with the midpoints between neighbouring doubles,
// as nearestFrom() of positions.cpp: up while the value is above the next
// midpoint, otherwise down while it is below the previous one; an exact
// midpoint goes to the even neighbour. against(m) is the exact sign of
// value - m (filtered first). Nothing when the start is not a usable double or
// the value is more than `steps` ulp away. The result depends only on the exact
// value, not on the start.
template <typename Against>
std::optional<double> nearestFrom(double start, const Against& against, int steps) {
    if (!std::isfinite(start) || std::fabs(start) > kLargestRounded) {
        return std::nullopt;
    }
    double result = canonicalZero(start);
    for (int moved = 0;; ++moved) {
        const double up = std::nextafter(result, kInfinity);
        const Sign side = against(midpoint(result, up));
        if (side == Sign::zero) {
            return canonicalZero(evenOf(result, up));
        }
        if (side == Sign::negative) {
            if (moved > 0) {
                return canonicalZero(result);
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
        const Sign side = against(midpoint(down, result));
        if (side == Sign::zero) {
            return canonicalZero(evenOf(down, result));
        }
        if (side == Sign::positive) {
            return canonicalZero(result);
        }
        if (moved == steps || std::fabs(down) > kLargestRounded) {
            return std::nullopt;
        }
        result = down;
    }
}

// The rounding from the filtered start, and when that is too far off (the
// filter lost its accuracy to cancellation), from the value that the tower
// gives within 2 ulp.
template <typename Against>
double nearest(double rough, double accurate, const Against& against) {
    if (const auto result = nearestFrom(rough, against, kRoughSteps)) {
        return *result;
    }
    const auto result = nearestFrom(accurate, against, std::numeric_limits<int>::max());
    assert(result.has_value() && "the value is within the range of the roundings");
    return result.value_or(accurate);
}

// x / w correctly rounded, with w > 0.
double roundedRatio(const Filtered& numbers, const TowerNumber& x, const Dyadic& w) {
    const Wide value = numbers.evaluate(x);
    if (!certainSign(value) && numbers.tower().sign(x) == Sign::zero) {
        return 0.0;
    }
    const auto against = [&](const Dyadic& m) {
        const Dyadic scaled = m * w;
        if (const auto certain = certainSign(subtract(value, wide(scaled)))) {
            return *certain;
        }
        return numbers.tower().compare(x, TowerNumber(scaled));
    };
    const double rough = (value.high + value.low) / w.toDouble();
    if (const auto result = nearestFrom(rough, against, kRoughSteps)) {
        return *result;
    }
    return nearest(kInfinity, numbers.tower().quotientToDouble(x, w), against);
}

// n / (w sqrt(k)) correctly rounded, with w > 0 and k > 0. The comparison with
// a midpoint m is the sign of n - m w sqrt(k): filtered first, then exactly
// from the signs and, when they agree, from n^2 - (m w)^2 k, so that no root of
// k is adjoined.
double roundedQuotient(const Filtered& numbers, const TowerNumber& n, const Dyadic& w,
                       const Dyadic& k) {
    const Wide value = numbers.evaluate(n);
    const auto filteredSign = certainSign(value);
    const Sign signN = filteredSign ? *filteredSign : numbers.tower().sign(n);
    if (signN == Sign::zero) {
        return 0.0;
    }
    const Wide root = squareRootOf(wide(k));
    const auto against = [&](const Dyadic& m) {
        const Dyadic scaled = m * w;
        if (const auto certain = certainSign(subtract(value, multiply(wide(scaled), root)))) {
            return *certain;
        }
        const Sign signM = m.sign();
        if (signN != signM) {
            return static_cast<int>(signN) > static_cast<int>(signM) ? Sign::positive
                                                                     : Sign::negative;
        }
        const Sign magnitudes =
            numbers.tower().compare(numbers.product(n, n), TowerNumber(scaled * scaled * k));
        return signN == Sign::positive ? magnitudes : negated(magnitudes);
    };
    const double rough = (value.high + value.low) / (w.toDouble() * (root.high + root.low));
    if (const auto result = nearestFrom(rough, against, kRoughSteps)) {
        return *result;
    }
    // A lower bound of sqrt(k) with 128 bits: the start is within a few ulp.
    return nearest(kInfinity, numbers.tower().quotientToDouble(n, w * floorSqrt(k, kRootBits)),
                   against);
}

// --- Positions of points of the tower ----------------------------------------

// A vector with coordinates of the tower.
struct TowerVector {
    TowerNumber x;
    TowerNumber y;
};

// g . v and cross(g, v) for a dyadic vector g.
TowerNumber dotWith(const ExactVector& g, const TowerVector& v) {
    return v.x * g.x + v.y * g.y;
}

TowerNumber crossWith(const ExactVector& g, const TowerVector& v) {
    return v.y * g.x - v.x * g.y;
}

// The direction of P = X / w from the centre c / W: X W - c w, which is
// P - C times w W > 0.
TowerVector fromCentre(const CircleSupport& circle, const TowerPoint& point) {
    return {point.x * circle.weight - TowerNumber(circle.centerX * point.weight),
            point.y * circle.weight - TowerNumber(circle.centerY * point.weight)};
}

// Straight curves.

ExactVector directionOf(const LineMeaning& line) {
    return minus(exactVector(line.end), exactVector(line.start));
}

// (P - O) . U times w, for a point O of doubles: X . U - w O . U.
TowerNumber alongFrom(const ExactVector& direction, Point2 origin, const TowerPoint& point) {
    return point.x * direction.x + point.y * direction.y -
           TowerNumber(point.weight * dot(direction, exactVector(origin)));
}

Location locateOnLine(const Filtered& numbers, const LineMeaning& line, const TowerPoint& point) {
    const auto direction = directionOf(line);
    switch (numbers.sign(alongFrom(direction, line.start, point))) {
        case Sign::negative:
            return Location::beforeStart;
        case Sign::zero:
            return Location::atStart;
        case Sign::positive:
            break;
    }
    switch (numbers.sign(alongFrom(direction, line.end, point))) {
        case Sign::negative:
            return Location::inside;
        case Sign::zero:
            return Location::atEnd;
        case Sign::positive:
            break;
    }
    return Location::afterEnd;
}

// (A - B) . U times wA wB > 0.
Sign compareOnLine(const Filtered& numbers, const LineMeaning& line, const TowerPoint& a,
                   const TowerPoint& b) {
    const auto direction = directionOf(line);
    const TowerNumber dx = a.x * b.weight - b.x * a.weight;
    const TowerNumber dy = a.y * b.weight - b.y * a.weight;
    return numbers.sign(dx * direction.x + dy * direction.y);
}

// (P - S) . U / |U|, correctly rounded.
double stationOnLine(const Filtered& numbers, const LineMeaning& line, const TowerPoint& point) {
    const auto direction = directionOf(line);
    return roundedQuotient(numbers, alongFrom(direction, line.start, point), point.weight,
                           dot(direction, direction));
}

// Circular curves.

TurnSector sectorFrom(const Filtered& numbers, const ArcMeaning& arc, const TowerVector& v) {
    return sectorOf(oriented(arc.turn, numbers.sign(crossWith(arc.startDirection, v))),
                    numbers.sign(dotWith(arc.startDirection, v)));
}

Location locateOnCircle(const Filtered& numbers, const ArcMeaning& arc, const TowerPoint& point) {
    assert(arc.span != ArcSpan::point);
    if (arc.span == ArcSpan::circle) {
        return Location::inside;
    }
    const auto v = fromCentre(arc.circle, point);
    const auto fromStart = sectorFrom(numbers, arc, v);
    // As in positions.cpp: the sign towards D1 is read only in its quarter.
    Sign toEnd = Sign::zero;
    if (arc.span == ArcSpan::partial && fromStart == arc.endSector && isOpenQuarter(fromStart)) {
        toEnd = oriented(arc.turn, negated(numbers.sign(crossWith(arc.endDirection, v))));
    }
    return locateOnArc(arc, fromStart, toEnd);
}

Sign compareOnCircle(const Filtered& numbers, const ArcMeaning& arc, const TowerPoint& a,
                     const TowerPoint& b) {
    const auto va = fromCentre(arc.circle, a);
    const auto vb = fromCentre(arc.circle, b);
    const auto sa = sectorFrom(numbers, arc, va);
    const auto sb = sectorFrom(numbers, arc, vb);
    Sign orientedCross = Sign::zero;
    if (sa == sb && isOpenQuarter(sa)) {
        orientedCross = oriented(
            arc.turn, numbers.compare(numbers.product(va.x, vb.y), numbers.product(va.y, vb.x)));
    }
    return compareTurns(sa, sb, orientedCross);
}

// turnAngle() of positions.cpp for a point of the tower: the quarter from the
// sector, then within it asin of the sine below the diagonal and pi/2 - asin of
// the cosine on and above it, with the same clamps. With v = V / (w W) and
// R = sqrt(rho) / W, g . v / (|D0| R) = (g . V) / (w sqrt(rho |D0|^2)).
double turnAngle(const Filtered& numbers, const ArcMeaning& arc, const TowerPoint& point) {
    const auto v = fromCentre(arc.circle, point);
    const auto sector = sectorFrom(numbers, arc, v);
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
    if (numbers.sign(dotWith(diagonal, v)) == Sign::negative) {
        const double sine = roundedQuotient(numbers, dotWith(next, v), point.weight, k);
        return std::min(start + arcsine(sine), middle);
    }
    const double cosine = roundedQuotient(numbers, dotWith(axis, v), point.weight, k);
    return std::max(std::min(start + (kQuarterTurn - arcsine(cosine)), end), middle);
}

double stationOnCircle(const Filtered& numbers, const ArcMeaning& arc, const TowerPoint& point) {
    // R = sqrt(rho) / W, rounded as positions.cpp rounds it.
    const double radius = correctlyRounded(Dyadic(), Dyadic(1.0), arc.circle.radiusSquared,
                                           arc.circle.weight, Dyadic(1.0));
    return radius * turnAngle(numbers, arc, point);
}

Location locateIn(const Filtered& numbers, const CurveMeaning& curve, const TowerPoint& point) {
    assert(!curve.degenerate);
    if (const auto* line = std::get_if<LineMeaning>(&curve.support)) {
        return locateOnLine(numbers, *line, point);
    }
    return locateOnCircle(numbers, std::get<ArcMeaning>(curve.support), point);
}

Sign compareIn(const Filtered& numbers, const CurveMeaning& curve, const TowerPoint& a,
               const TowerPoint& b) {
    assert(!curve.degenerate);
    if (const auto* line = std::get_if<LineMeaning>(&curve.support)) {
        return compareOnLine(numbers, *line, a, b);
    }
    return compareOnCircle(numbers, std::get<ArcMeaning>(curve.support), a, b);
}

double stationIn(const Filtered& numbers, const CurveMeaning& curve, const TowerPoint& point) {
    assert(!curve.degenerate);
    if (const auto* line = std::get_if<LineMeaning>(&curve.support)) {
        return stationOnLine(numbers, *line, point);
    }
    return stationOnCircle(numbers, std::get<ArcMeaning>(curve.support), point);
}

Point2 nearestIn(const Filtered& numbers, const TowerPoint& point) {
    return Point2{roundedRatio(numbers, point.x, point.weight),
                  roundedRatio(numbers, point.y, point.weight)};
}

// --- Tangent circles ---------------------------------------------------------

// A parallel of a curve at distance r, with what the circles centred on it
// share.
struct Parallel {
    OffsetCarrier carrier;
    /// The side of the curve on which the tangent circles centred here lie.
    Side side = Side::left;
    /// For a circular curve, the factor sqrt(rho) / n = factor / denominator
    /// (denominator > 0) from P - C to T - C.
    TowerNumber factor;
    Dyadic denominator = Dyadic(1.0);
};

Diagnosis towerFull() {
    return Diagnosis{DiagnosisCode::outOfDomain, 2,
                     "the exact numbers of the tangent circles need more roots than the tower "
                     "holds"};
}

// The two parallels of a straight curve, at +r (left) and -r (right).
std::optional<std::vector<Parallel>> lineParallels(Tower& tower, const CurveMeaning& meaning,
                                                   double radius) {
    std::vector<Parallel> parallels;
    for (const double distance : {radius, -radius}) {
        auto offset = offsetCarrier(tower, meaning, distance);
        if (offset.status != OffsetStatus::done) {
            // A line never vanishes: only a full tower stops here.
            return std::nullopt;
        }
        parallels.push_back(Parallel{std::move(offset.carrier),
                                     distance > 0.0 ? Side::left : Side::right, TowerNumber(),
                                     Dyadic(1.0)});
    }
    return parallels;
}

// The parallels of a circular curve, of radius R + r and |R - r|; none for
// |R - r| == 0 (see the top of this file).
std::optional<std::vector<Parallel>> circleParallels(Tower& tower, const ArcMeaning& arc,
                                                     double radius) {
    const auto& circle = arc.circle;
    const auto root = tower.squareRoot(TowerNumber(circle.radiusSquared));
    if (!root) {
        return std::nullopt;
    }
    const Dyadic shift = Dyadic(radius) * circle.weight;
    std::vector<Parallel> parallels;
    for (const bool outside : {true, false}) {
        const Dyadic a = outside ? shift : -shift;
        const TowerNumber n = *root + TowerNumber(a);
        const Sign sign = tower.sign(n);
        if (sign == Sign::zero) {
            continue;
        }
        const bool inside = !outside && sign == Sign::positive;
        const bool counterclockwise = arc.turn == Turn::counterclockwise;
        Parallel parallel{OffsetCircle{circle.centerX, circle.centerY, circle.weight,
                                       tower.product(n, n), arc.turn},
                          inside == counterclockwise ? Side::left : Side::right, TowerNumber(),
                          Dyadic(1.0)};
        if (root->isRational()) {
            parallel.factor = *root;
            parallel.denominator = n.coefficient(0);
        } else {
            parallel.factor = TowerNumber(circle.radiusSquared) - *root * a;
            parallel.denominator = circle.radiusSquared - a * a;
        }
        assert(!parallel.denominator.isZero());
        if (parallel.denominator.sign() == Sign::negative) {
            parallel.factor = -parallel.factor;
            parallel.denominator = -parallel.denominator;
        }
        parallels.push_back(std::move(parallel));
    }
    return parallels;
}

std::optional<std::vector<Parallel>> parallelsOf(Tower& tower, const CurveMeaning& meaning,
                                                 double radius) {
    if (std::holds_alternative<LineMeaning>(meaning.support)) {
        return lineParallels(tower, meaning, radius);
    }
    return circleParallels(tower, std::get<ArcMeaning>(meaning.support), radius);
}

// The point of tangency of the circle centred at `centre` on a parallel of
// the curve (see the top of this file).
TowerPoint tangencyPoint(const Tower& tower, const CurveMeaning& meaning, const Parallel& parallel,
                         const TowerPoint& centre) {
    if (const auto* line = std::get_if<LineMeaning>(&meaning.support)) {
        const auto direction = directionOf(*line);
        const auto start = exactVector(line->start);
        const Dyadic squaredLength = dot(direction, direction);
        const TowerNumber along = alongFrom(direction, line->start, centre);
        const Dyadic scale = centre.weight * squaredLength;
        return TowerPoint{TowerNumber(start.x * scale) + along * direction.x,
                          TowerNumber(start.y * scale) + along * direction.y, scale};
    }
    const auto& circle = std::get<ArcMeaning>(meaning.support).circle;
    const auto v = fromCentre(circle, centre);
    const Dyadic scale = centre.weight * parallel.denominator;
    return TowerPoint{TowerNumber(circle.centerX * scale) + tower.product(v.x, parallel.factor),
                      TowerNumber(circle.centerY * scale) + tower.product(v.y, parallel.factor),
                      scale * circle.weight};
}

// beyondDomain() of offset_carriers.hpp, with the filter first: a coordinate
// whose bound keeps it below robust::kMaxCoordinate is inside (the weight in
// double is within u of the exact one, hence the margin).
bool centreBeyondDomain(const Filtered& numbers, const TowerPoint& point) {
    const double limit = robust::kMaxCoordinate * point.weight.toDouble() * (1.0 - 0x1p-50);
    for (const auto* coordinate : {&point.x, &point.y}) {
        const Wide value = numbers.evaluate(*coordinate);
        if (!(magnitudeOf(value) + value.error < limit)) {
            return beyondDomain(numbers.tower(), point);
        }
    }
    return false;
}

// The numbers of a tower rewritten in another one, whose roots include those
// of the first: each root of the source is adjoined to the target (the same
// radicand gives the same root), and a number is rebuilt from its
// coefficients. Used to compare points of two candidates, which have towers of
// their own.
class Lift {
public:
    // Nothing when a root does not fit in the target.
    static std::optional<Lift> into(Tower& target, const Tower& source) {
        Lift lift(target);
        for (const auto& radicand : source.radicands()) {
            const auto root = target.squareRoot(lift.number(radicand));
            if (!root) {
                return std::nullopt;
            }
            lift.m_roots.push_back(*root);
        }
        return lift;
    }

    [[nodiscard]] TowerNumber number(const TowerNumber& x) const {
        const auto coefficients = x.coefficients();
        TowerNumber result;
        for (std::size_t mask = 0; mask < coefficients.size(); ++mask) {
            if (coefficients[mask].isZero()) {
                continue;
            }
            TowerNumber term(coefficients[mask]);
            for (std::size_t root = 0; (mask >> root) != 0; ++root) {
                if (((mask >> root) & 1U) != 0) {
                    term = m_target->product(term, m_roots[root]);
                }
            }
            result = result + term;
        }
        return result;
    }

    [[nodiscard]] TowerPoint point(const TowerPoint& p) const {
        return TowerPoint{number(p.x), number(p.y), p.weight};
    }

private:
    explicit Lift(const Tower& target) : m_target(&target) {}

    const Tower* m_target;
    std::vector<TowerNumber> m_roots;
};

// Which exact point of a candidate a comparison reads.
enum class Part : std::uint8_t { onFirst, onSecond, centre };

const TowerPoint& partOf(const TangentCandidate& candidate, Part part) {
    switch (part) {
        case Part::onFirst:
            return candidate.onFirst;
        case Part::onSecond:
            return candidate.onSecond;
        case Part::centre:
            break;
    }
    return candidate.center;
}

// The order of the same part of two candidates, in one tower that holds the
// roots of both: along a curve, or for the centres by x and then by y.
Sign compareAcross(const TangentCandidate& a, const TangentCandidate& b, Part part,
                   const CurveMeaning* curve) {
    Tower common;
    const auto liftA = Lift::into(common, a.tower);
    const auto liftB = liftA ? Lift::into(common, b.tower) : std::nullopt;
    // The two towers share the roots of the curves, so the common one holds at
    // most their two roots and the two discriminants.
    assert(liftA && liftB && "the common tower holds the roots of both candidates");
    if (!liftA || !liftB) {
        return Sign::zero;
    }
    const auto pointA = liftA->point(partOf(a, part));
    const auto pointB = liftB->point(partOf(b, part));
    const Filtered numbers(common);
    if (curve != nullptr) {
        return compareIn(numbers, *curve, pointA, pointB);
    }
    const Sign byX = numbers.compare(pointA.x * pointB.weight, pointB.x * pointA.weight);
    if (byX != Sign::zero) {
        return byX;
    }
    return numbers.compare(pointA.y * pointB.weight, pointB.y * pointA.weight);
}

// The order of two candidates: the order of the rounded values when they
// differ, since the stations and the correctly rounded coordinates never
// decrease along the exact order; the exact order otherwise.
Sign compareRounded(double a, double b) {
    if (a < b) {
        return Sign::negative;
    }
    return b < a ? Sign::positive : Sign::zero;
}

bool comesFirst(const TangentCandidate& a, const TangentCandidate& b, const CurveMeaning& first,
                const CurveMeaning& second) {
    const auto order = [&](double roundedA, double roundedB, Part part, const CurveMeaning* curve) {
        const Sign rounded = compareRounded(roundedA, roundedB);
        return rounded != Sign::zero ? rounded : compareAcross(a, b, part, curve);
    };
    const Sign alongFirst = order(a.circle.onFirst.position.station,
                                  b.circle.onFirst.position.station, Part::onFirst, &first);
    if (alongFirst != Sign::zero) {
        return alongFirst == Sign::negative;
    }
    const Sign alongSecond = order(a.circle.onSecond.position.station,
                                   b.circle.onSecond.position.station, Part::onSecond, &second);
    if (alongSecond != Sign::zero) {
        return alongSecond == Sign::negative;
    }
    // Different candidates have different centres; the rounded x decides when
    // it differs, then the exact order by x and y.
    const Sign byX = compareRounded(a.circle.center.x, b.circle.center.x);
    if (byX != Sign::zero) {
        return byX == Sign::negative;
    }
    return compareAcross(a, b, Part::centre, nullptr) == Sign::negative;
}

// The candidates of one pair of parallels, in a tower of their own: the roots
// of the two curves and the discriminant of the pair. Sets `infinitelyMany`
// when the two parallels are the same line or circle.
Result<std::vector<TangentCandidate>> pairCandidates(const CurveMeaning& first,
                                                     const CurveMeaning& second, double radius,
                                                     std::size_t indexFirst,
                                                     std::size_t indexSecond,
                                                     bool& infinitelyMany) {
    Tower tower;
    const auto parallelsOfFirst = parallelsOf(tower, first, radius);
    const auto parallelsOfSecond =
        parallelsOfFirst ? parallelsOf(tower, second, radius) : std::nullopt;
    if (!parallelsOfFirst || !parallelsOfSecond) {
        return towerFull();
    }
    std::vector<TangentCandidate> candidates;
    if (indexFirst >= parallelsOfFirst->size() || indexSecond >= parallelsOfSecond->size()) {
        return candidates;
    }
    const auto& onFirst = (*parallelsOfFirst)[indexFirst];
    const auto& onSecond = (*parallelsOfSecond)[indexSecond];
    const auto meeting = meetOffset(tower, onFirst.carrier, onSecond.carrier);
    if (!meeting) {
        return towerFull();
    }
    if (meeting->sameCarrier) {
        infinitelyMany = true;
        return candidates;
    }
    const Filtered numbers(tower);
    for (const auto& found : meeting->found()) {
        const auto& centre = found.point;
        if (centreBeyondDomain(numbers, centre)) {
            return Diagnosis{DiagnosisCode::outOfDomain, 2,
                             "the centre of a tangent circle is beyond robust::kMaxCoordinate"};
        }
        TangentCandidate candidate{centre, tangencyPoint(tower, first, onFirst, centre),
                                   tangencyPoint(tower, second, onSecond, centre), TangentCircle{},
                                   Tower()};
        candidate.circle.center = nearestIn(numbers, centre);
        candidate.circle.onFirst =
            TangencyPoint{nearestIn(numbers, candidate.onFirst),
                          CurvePosition{locateIn(numbers, first, candidate.onFirst),
                                        stationIn(numbers, first, candidate.onFirst)}};
        candidate.circle.onSecond =
            TangencyPoint{nearestIn(numbers, candidate.onSecond),
                          CurvePosition{locateIn(numbers, second, candidate.onSecond),
                                        stationIn(numbers, second, candidate.onSecond)}};
        candidate.circle.sideOfFirst = onFirst.side;
        candidate.circle.sideOfSecond = onSecond.side;
        candidates.push_back(std::move(candidate));
    }
    for (auto& candidate : candidates) {
        candidate.tower = tower;
    }
    return candidates;
}

}  // namespace

Location locate(const Tower& tower, const CurveMeaning& curve, const TowerPoint& point) {
    return locateIn(Filtered(tower), curve, point);
}

Sign compareAlong(const Tower& tower, const CurveMeaning& curve, const TowerPoint& a,
                  const TowerPoint& b) {
    return compareIn(Filtered(tower), curve, a, b);
}

double station(const Tower& tower, const CurveMeaning& curve, const TowerPoint& point) {
    return stationIn(Filtered(tower), curve, point);
}

CurvePosition positionOn(const Tower& tower, const CurveMeaning& curve, const TowerPoint& point) {
    const Filtered numbers(tower);
    return CurvePosition{locateIn(numbers, curve, point), stationIn(numbers, curve, point)};
}

Point2 nearestPoint(const Tower& tower, const TowerPoint& point) {
    return nearestIn(Filtered(tower), point);
}

std::optional<Diagnosis> checkRadius(double radius, std::size_t input) {
    if (!std::isfinite(radius) || radius < 0.0) {
        return Diagnosis{DiagnosisCode::invalidValue, input,
                         "the radius is negative, NaN or infinite"};
    }
    if (radius > robust::kMaxCoordinate) {
        return Diagnosis{DiagnosisCode::outOfDomain, input,
                         "the radius is beyond robust::kMaxCoordinate"};
    }
    return std::nullopt;
}

Result<TangentCandidates> tangentCandidates(const CurveMeaning& first, const CurveMeaning& second,
                                            double radius) {
    assert(!first.degenerate && !second.degenerate);
    assert(std::isfinite(radius) && radius > 0.0 && radius <= robust::kMaxCoordinate);
    // Every pair first, so that infinitely many centres are told before any
    // diagnosis about a centre. A curve has at most two parallels.
    constexpr std::size_t kParallels = 2;
    TangentCandidates result;
    std::vector<Result<std::vector<TangentCandidate>>> pairs;
    for (std::size_t i = 0; i < kParallels; ++i) {
        for (std::size_t j = 0; j < kParallels; ++j) {
            pairs.push_back(pairCandidates(first, second, radius, i, j, result.infinitelyMany));
        }
    }
    if (result.infinitelyMany) {
        return result;
    }
    for (auto& pair : pairs) {
        if (!pair.hasValue()) {
            return pair.diagnosis();
        }
        for (auto& candidate : std::move(pair).value()) {
            result.candidates.push_back(std::move(candidate));
        }
    }
    std::sort(result.candidates.begin(), result.candidates.end(),
              [&](const TangentCandidate& a, const TangentCandidate& b) {
                  return comesFirst(a, b, first, second);
              });
    return result;
}

}  // namespace detail

namespace {

using detail::ArcMeaning;
using detail::CircleSupport;
using detail::Dyadic;
using detail::ExactPoint;
using detail::ExactVector;
using robust::Sign;

// The point of tangentsFrom is input 1.
constexpr std::size_t kPointInput = 1;

// The thresholds of the filter of the placement (see the top of this file):
// 16 u of the computed magnitudes, and an absolute term above the underflow of
// the three squares.
constexpr double kFilterRelative = 0x1p-49;
constexpr double kFilterAbsolute = 0x1p-1069;

std::optional<Diagnosis> checkPoint(Point2 point) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
        return Diagnosis{DiagnosisCode::invalidValue, kPointInput,
                         "a coordinate of the point is NaN or infinite"};
    }
    if (std::fabs(point.x) > robust::kMaxCoordinate ||
        std::fabs(point.y) > robust::kMaxCoordinate) {
        return Diagnosis{DiagnosisCode::outOfDomain, kPointInput,
                         "a coordinate of the point is beyond robust::kMaxCoordinate"};
    }
    return std::nullopt;
}

// The sign of |Q - C|^2 - R^2 in floating point, when it is certain.
std::optional<Sign> filteredSign(Point2 centre, double radius, Point2 point) {
    const double dx = point.x - centre.x;
    const double dy = point.y - centre.y;
    const double squares = (dx * dx) + (dy * dy);
    const double square = radius * radius;
    const double value = squares - square;
    const double bound = (kFilterRelative * (squares + square)) + kFilterAbsolute;
    if (value > bound) {
        return Sign::positive;
    }
    if (value < -bound) {
        return Sign::negative;
    }
    return std::nullopt;
}

Placement placementOf(const Curve& curve, const CircleSupport& circle, Point2 point) {
    std::optional<Sign> sign;
    if (const auto* arc = std::get_if<Arc>(&curve)) {
        sign = filteredSign(arc->center, arc->radius, point);
    } else if (const auto* whole = std::get_if<Circle>(&curve)) {
        sign = filteredSign(whole->center, whole->radius, point);
    }
    if (!sign) {
        sign = detail::circleSign(circle, detail::exactPoint(point));
    }
    switch (*sign) {
        case Sign::negative:
            return Placement::inside;
        case Sign::zero:
            return Placement::on;
        case Sign::positive:
            break;
    }
    return Placement::outside;
}

// The two exact points of tangency from a point outside the circle (see the
// top of this file).
std::array<ExactPoint, 2> tangencyPoints(const CircleSupport& circle, Point2 point) {
    const ExactVector centre{circle.centerX, circle.centerY};
    const auto exact = detail::exactVector(point);
    const ExactVector q{exact.x * circle.weight - centre.x, exact.y * circle.weight - centre.y};
    const Dyadic squared = q.x * q.x + q.y * q.y;
    const Dyadic rho = circle.radiusSquared;
    const ExactVector rational{centre.x * squared + q.x * rho, centre.y * squared + q.y * rho};
    const Dyadic radicand = rho * (squared - rho);
    const Dyadic weight = circle.weight * squared;
    return {ExactPoint{rational, ExactVector{-q.y, q.x}, radicand, weight},
            ExactPoint{rational, ExactVector{q.y, -q.x}, radicand, weight}};
}

}  // namespace

Result<TangentsFromPoint> tangentsFrom(Point2 point, const Curve& curve, Extension extension) {
    auto checked = detail::checkedMeaning(curve, 0);
    if (!checked.hasValue()) {
        return checked.diagnosis();
    }
    const auto& meaning = checked.value();
    const auto* arc = std::get_if<ArcMeaning>(&meaning.support);
    if (arc == nullptr) {
        return Diagnosis{DiagnosisCode::notCircular, 0,
                         "tangent lines from a point need a circular curve"};
    }
    if (const auto diagnosis = checkPoint(point)) {
        return *diagnosis;
    }
    TangentsFromPoint result;
    result.placement = placementOf(curve, arc->circle, point);
    switch (result.placement) {
        case Placement::inside:
            break;
        case Placement::on: {
            const auto position = detail::positionOn(meaning, detail::exactPoint(point));
            if (detail::takesPart(position.location, extension)) {
                result.points.push_back(TangencyPoint{point, position});
            }
            break;
        }
        case Placement::outside: {
            auto points = tangencyPoints(arc->circle, point);
            if (detail::compareAlong(meaning, points[1], points[0]) == Sign::negative) {
                std::swap(points[0], points[1]);
            }
            for (const auto& exact : points) {
                const auto position = detail::positionOn(meaning, exact);
                if (detail::takesPart(position.location, extension)) {
                    result.points.push_back(TangencyPoint{detail::nearestPoint(exact), position});
                }
            }
            break;
        }
    }
    return result;
}

Result<TangentCircles> tangentCircles(const Curve& first, const Curve& second, double radius) {
    auto firstMeaning = detail::checkedMeaning(first, 0);
    if (!firstMeaning.hasValue()) {
        return firstMeaning.diagnosis();
    }
    auto secondMeaning = detail::checkedMeaning(second, 1);
    if (!secondMeaning.hasValue()) {
        return secondMeaning.diagnosis();
    }
    if (const auto diagnosis = detail::checkRadius(radius, 2)) {
        return *diagnosis;
    }
    if (radius == 0.0) {
        return Diagnosis{DiagnosisCode::degenerateCurve, 2,
                         "a tangent circle of radius zero is a single point"};
    }
    auto candidates =
        detail::tangentCandidates(firstMeaning.value(), secondMeaning.value(), radius);
    if (!candidates.hasValue()) {
        return candidates.diagnosis();
    }
    TangentCircles result;
    result.infinitelyMany = candidates.value().infinitelyMany;
    result.circles.reserve(candidates.value().candidates.size());
    for (const auto& candidate : candidates.value().candidates) {
        result.circles.push_back(candidate.circle);
    }
    return result;
}

}  // namespace solid_space::geom2d
