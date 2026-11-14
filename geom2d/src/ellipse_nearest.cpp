// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// nearest() of ellipse.hpp (stage 0.3, package PS8; ADR 0017, points 5, 8
// and 9).
//
// The feet. The ellipse is the image of the unit circle under
// F(c, s) = center + c * a + s * b, with a = majorAxis and b the exact minor
// axis (ellipse_meaning.hpp); a . b = 0. For q = p - center the squared
// distance from p to F(c, s) is
//     |q|^2 - 2 P c - 2 Q s + alpha c^2 + beta s^2,
// with P = q . a, Q = q . b, alpha = a . a, beta = b . b, all exact dyadic
// numbers. On the unit circle (c, s) = (cos t, sin t) its derivative in t is
// 2 (P s - Q c - gamma c s), with gamma = alpha - beta >= 0, and the feet of
// the perpendiculars from p are its zeros. With the rational form
// c = (1 - u^2) / (1 + u^2), s = 2 u / (1 + u^2) (u = tan(t / 2)) and the
// positive factor (1 + u^2)^2 they are the real roots of the quartic
//     g(u) = Q u^4 + 2 (P + gamma) u^3 + 2 (P - gamma) u - Q,
// whose coefficients are exact: the intersection with the hyperbola of
// Apollonius in another form. The only direction the rational form misses is
// (-1, 0) (u infinite), a foot exactly when Q == 0. g is the zero polynomial
// only when Q == P == gamma == 0: a circle seen from its centre, where every
// point is a foot. For |u| > 1 the same feet are the roots v = 1 / u of
//     h(v) = v^4 g(1 / v) = -Q v^4 + 2 (P - gamma) v^3 + 2 (P + gamma) v + Q.
//
// How many. P and Q are, up to the positive factors |a| and |b|, the
// coordinates of p in the frame of the axes, and gamma = |a|^2 - |b|^2, so
// the evolute of the ellipse is P^(2/3) + Q^(2/3) = gamma^(2/3), that is
//     E = (P^2 + Q^2 - gamma^2)^3 + 27 P^2 Q^2 gamma^2 = 0.
// From a point strictly inside it (E < 0) there are four distinct feet,
// strictly outside (E > 0) two; on it (E == 0) two of them are one multiple
// root of g. E is computed exactly.
//
// Two paths, both deterministic, both within the error below.
// - The filter, for Q != 0 and E != 0 (simple roots, none at infinity): the
//   roots of g on [-1, 1] and of h on (-1, 1) are found in doubles, then each
//   is checked in exact arithmetic: the polynomial changes sign (or vanishes)
//   between two adjacent doubles near the root found, and these brackets do
//   not overlap. When the brackets are as many as E says, every foot is in one
//   of them, within one ulp of its double. Whether a foot lies on the arc is
//   then read from doubles with a margin far above their error; inside that
//   margin, or when any check fails, the filter gives up.
// - The exact path, otherwise: the roots of g are isolated exactly with
//   exact/polynomial.hpp (Sturm sequences), with their multiplicities, and
//   rounded with exact::toDouble(); whether a foot lies on the arc is decided
//   exactly with exact::signAt() on the quadratics of the cross and dot
//   products with D0 and D1. Near the evolute two or three feet come
//   together into a multiple root, and nothing here depends on how close
//   they are: no root is located by a floating-point search alone.
// In both, a foot exactly at an end is left to the end itself, so that an end
// is always returned with the bits of endpoints(). The candidates are the
// ends of an arc and the feet strictly inside it (every foot of a whole
// ellipse).
//
// The construction and its error (ADR 0013, points 2 and 4). Below, ulp is
// 2^-52 and N is the largest of M, |p.x| and |p.y|; every coordinate of a, b
// and center is at most M, so every coordinate of a point of the carrying
// ellipse is at most 3 M.
// - The root, u or v, is a double w' within one ulp of the exact w:
//   |w' - w| <= 2^-52 |w'| (+ 2^-1074). For |u| > 1 the exact path uses
//   v = 1 / u', within two ulp. Then, with |w| <= 1,
//   c = (1 - w^2) / (1 + w^2), s = 2 w / (1 + w^2) (with u), or
//   c = (w^2 - 1) / (w^2 + 1), s = 2 w / (w^2 + 1) (with v): the derivatives
//   of c and s in w are at most 2 in absolute value, the rounding of w adds
//   at most 2 ulp of a number at most 1, and the four operations at most
//   4 ulp: |c' - c|, |s' - s| <= 2^-48.
// - The point is center + c' a + s' b', with b' the doubles of b
//   (ratio * -a.y and ratio * a.x, each rounded: half an ulp of a number at
//   most M). Each coordinate is off by at most 2^-48 M from c' and s' each,
//   plus half an ulp of b' and three roundings of sums at most 3 M: below
//   2^-46 M per coordinate, so |point - q| <= 2^-45.5 M.
// - An end is the point of endpoints(), within kRelativePointError = 2^-48
//   of the exact end, relative to M (curves.hpp).
// - The distance is sqrt(dx^2 + dy^2) with dx = p.x - point.x, dy likewise.
//   The two subtractions add half an ulp of a number at most 4 N each, and
//   the sum, the squares and the square root a relative error below 3 ulp of
//   a distance at most 6 N: in all, |distance - |p - point|| <= 2^-48 N, so
//   |distance - |p - q|| <= 2^-45.5 M + 2^-48 N < 2^-45 N.
// So point and distance are each within 2^-45 N of the exact candidate q,
// half of kRelativeNearestError * N. The candidate with the smallest computed
// distance is chosen (the first in a fixed order on equal doubles: start,
// end, then the feet in the order of their roots): its exact distance exceeds
// that of every other candidate by at most twice 2^-45 N, that is
// kRelativeNearestError * N, and d* is the distance of some candidate. Which
// one among candidates closer than that is the band of ADR 0013, point 4;
// beyond it the choice is determined. The same data take the same path and
// the same doubles in the same order: the result is deterministic (ADR 0008,
// point 5).
//
// The margin of the filter on the arc. The exact direction of a foot is
// (c, s) up to a positive factor, and D0 and D1 are doubles of length within
// 2^-51 of 1 (robust::sinCos). The cross and dot products of (c', s') with
// them, computed in doubles, are within 2^-48 * 2 + 2^-52 * 2 < 2^-46 of the
// exact ones scaled to the unit direction: with a magnitude above
// kDirectionMargin = 2^-40 their signs are the exact ones.
//
// Cost (ADR 0017, point 9). The exact path builds Sturm sequences of a
// quartic whose coefficients have hundreds of bits; the filter needs a few
// exact evaluations of g at doubles and of E, and nothing else exact.

#include "curve_meaning.hpp"
#include "ellipse_meaning.hpp"
#include "exact/polynomial.hpp"
#include "solid_space/geom2d/ellipse.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace solid_space::geom2d {
namespace {

using detail::ExactVector;
using exact::Dyadic;
using exact::Polynomial;
using robust::Sign;

const Dyadic kTwo{2.0};

// Above the error of the products of the filter with D0 and D1 (2^-46, see
// the top of the file), with room to spare.
constexpr double kDirectionMargin = 0x1p-40;

Dyadic dot(const ExactVector& a, const ExactVector& b) {
    return a.x * b.x + a.y * b.y;
}

std::optional<Diagnosis> checkPoint(Point2 p) {
    if (!std::isfinite(p.x) || !std::isfinite(p.y)) {
        return Diagnosis{DiagnosisCode::invalidValue, 1,
                         "a coordinate of the point is NaN or infinite"};
    }
    if (std::fabs(p.x) > robust::kMaxCoordinate || std::fabs(p.y) > robust::kMaxCoordinate) {
        return Diagnosis{DiagnosisCode::outOfDomain, 1,
                         "a coordinate of the point is beyond robust::kMaxCoordinate"};
    }
    return std::nullopt;
}

Sign signOf(double value) {
    if (value > 0.0) {
        return Sign::positive;
    }
    return value < 0.0 ? Sign::negative : Sign::zero;
}

// The direction (1 - u^2, 2 u) of the rational form, crossed with and dotted
// with a dyadic direction d, as quadratics in u:
//     d x (1 - u^2, 2 u) = d.y u^2 + 2 d.x u - d.y,
//     d . (1 - u^2, 2 u) = -d.x u^2 + 2 d.y u + d.x,
//     (1 - u^2, 2 u) x d = -d.y u^2 - 2 d.x u + d.y.
Polynomial crossFrom(const ExactVector& d) {
    return Polynomial({-d.y, kTwo * d.x, d.y});
}

Polynomial dotWith(const ExactVector& d) {
    return Polynomial({d.x, kTwo * d.y, -d.x});
}

// Where the direction of the rational form at an exact root lies on the unit
// arc. On a whole ellipse every foot is a candidate and the answer is not
// read, so the exact signs are skipped.
Location locateRoot(const detail::ArcMeaning& arc, const exact::RealRoot& root) {
    if (arc.span != detail::ArcSpan::partial) {
        return Location::inside;
    }
    const auto fromStart = detail::sectorOf(
        detail::oriented(arc.turn, exact::signAt(crossFrom(arc.startDirection), root)),
        exact::signAt(dotWith(arc.startDirection), root));
    const auto toEnd =
        detail::oriented(arc.turn, exact::signAt(-crossFrom(arc.endDirection), root));
    return detail::locateOnArc(arc, fromStart, toEnd);
}

// Where a unit direction of doubles lies on the unit arc, read from doubles
// (see the margin at the top); nothing when a product is inside the margin.
std::optional<Location> locateUnit(const detail::ArcMeaning& arc, Point2 unit) {
    if (arc.span != detail::ArcSpan::partial) {
        return Location::inside;
    }
    const Point2 start{arc.startDirection.x.toDouble(), arc.startDirection.y.toDouble()};
    const Point2 end{arc.endDirection.x.toDouble(), arc.endDirection.y.toDouble()};
    const double fromStart = start.x * unit.y - start.y * unit.x;
    const double along = start.x * unit.x + start.y * unit.y;
    const double toEnd = unit.x * end.y - unit.y * end.x;
    if (std::fabs(fromStart) <= kDirectionMargin || std::fabs(along) <= kDirectionMargin ||
        std::fabs(toEnd) <= kDirectionMargin) {
        return std::nullopt;
    }
    return detail::locateOnArc(
        arc, detail::sectorOf(detail::oriented(arc.turn, signOf(fromStart)), signOf(along)),
        detail::oriented(arc.turn, signOf(toEnd)));
}

// True when a foot at this location is a candidate of its own: strictly
// inside an arc, anywhere on a whole ellipse (its start too, still inside
// for nearest()). A foot at an end is the end, already a candidate.
bool footCandidate(const detail::ArcMeaning& arc, Location location) {
    if (arc.span != detail::ArcSpan::partial) {
        return true;
    }
    return location == Location::inside;
}

// The unit point (c, s) at u, or at v = 1 / u (see the error analysis at the
// top). Both need |w| <= 1 for the bound.
Point2 unitPointAtU(double u) {
    const double square = u * u;
    const double denominator = 1.0 + square;
    return {(1.0 - square) / denominator, (2.0 * u) / denominator};
}

Point2 unitPointAtV(double v) {
    const double square = v * v;
    const double denominator = square + 1.0;
    return {(square - 1.0) / denominator, (2.0 * v) / denominator};
}

Point2 unitPointAt(double u) {
    return std::fabs(u) <= 1.0 ? unitPointAtU(u) : unitPointAtV(1.0 / u);
}

// A foot of the perpendicular, built before knowing whether it lies on the
// arc: that question is asked only of the feet that could be chosen.
struct Foot {
    Point2 unit;
    Point2 point;
    double distance = 0.0;
    // The exact root of g on the exact path; none for the direction (-1, 0)
    // and on the filter.
    std::optional<exact::RealRoot> root;
};

class Chooser {
public:
    Chooser(const Ellipse& ellipse, Point2 p)
        : m_ellipse(ellipse),
          m_p(p),
          m_minor{-ellipse.ratio * ellipse.majorAxis.y, ellipse.ratio * ellipse.majorAxis.x} {}

    // F(c, s) in doubles, b rounded from the doubles of a and ratio.
    [[nodiscard]] Foot foot(Point2 unit, std::optional<exact::RealRoot> root = std::nullopt) const {
        const Point2 point{
            m_ellipse.center.x + (unit.x * m_ellipse.majorAxis.x + unit.y * m_minor.x),
            m_ellipse.center.y + (unit.x * m_ellipse.majorAxis.y + unit.y * m_minor.y)};
        return {unit, point, distanceTo(point), std::move(root)};
    }

    [[nodiscard]] double distanceTo(Point2 point) const {
        const double dx = m_p.x - point.x;
        const double dy = m_p.y - point.y;
        return std::sqrt(dx * dx + dy * dy);
    }

    // True when a candidate at this distance would replace the best one: on
    // equal doubles the earlier one stays.
    [[nodiscard]] bool wouldTake(double distance) const {
        return !m_found || distance < m_best.distance;
    }

    void offer(Point2 point, double distance, Location location) {
        if (wouldTake(distance)) {
            m_best = EllipseNearest{point, distance, location};
            m_found = true;
        }
    }

    void offer(Point2 point, Location location) { offer(point, distanceTo(point), location); }

    [[nodiscard]] EllipseNearest best() const {
        assert(m_found);
        return m_best;
    }

private:
    const Ellipse& m_ellipse;
    Point2 m_p;
    Point2 m_minor;
    EllipseNearest m_best;
    bool m_found = false;
};

// Offers the feet as if each were offered in its fixed order with its
// position known: a stable sort by distance keeps that order among equal
// doubles, and the first foot on the curve that beats the best candidate so
// far is the one the fixed order would keep. `locate` gives the position of
// a foot, or nothing when it cannot tell: then the answer is false, and what
// was offered must be thrown away.
template <typename Locate>
bool offerFeet(Chooser& chooser, const detail::ArcMeaning& arc, std::vector<Foot> feet,
               Locate locate) {
    std::ranges::stable_sort(feet, std::ranges::less{}, &Foot::distance);
    for (const auto& foot : feet) {
        if (!chooser.wouldTake(foot.distance)) {
            break;
        }
        const std::optional<Location> location = locate(foot);
        if (!location) {
            return false;
        }
        if (footCandidate(arc, *location)) {
            chooser.offer(foot.point, foot.distance, Location::inside);
            break;
        }
    }
    return true;
}

// ---- The filter ----

// The coefficients of a quartic in doubles, lowest degree first.
using Coefficients = std::array<double, 5>;

double evaluate(std::span<const double> coefficients, double x) {
    double value = 0.0;
    for (std::size_t i = coefficients.size(); i-- > 0;) {
        value = value * x + coefficients[i];
    }
    return value;
}

// The root in [low, high] of a polynomial of doubles whose values at the ends
// have opposite signs, by bisection down to adjacent doubles.
double bisect(std::span<const double> polynomial, double low, double high, double atLow) {
    const bool risingFromLow = atLow < 0.0;
    for (;;) {
        const double middle = 0.5 * low + 0.5 * high;
        if (middle <= low || middle >= high) {
            break;
        }
        const double atMiddle = evaluate(polynomial, middle);
        if (atMiddle == 0.0) {
            return middle;
        }
        if ((atMiddle < 0.0) == risingFromLow) {
            low = middle;
        } else {
            high = middle;
        }
    }
    return std::fabs(evaluate(polynomial, low)) <= std::fabs(evaluate(polynomial, high)) ? low
                                                                                         : high;
}

// The roots in [lower, upper] of a polynomial of doubles, found where its
// value changes sign between consecutive critical points (the roots of its
// derivative, found the same way) and refined by bisection. A root of even
// multiplicity, or one lost to rounding, is missed: the exact check that
// follows notices it, because the count is then short. A root found twice
// gives overlapping brackets, which the check refuses too.
// The recursion goes down the degree: at most four levels.
// NOLINTNEXTLINE(misc-no-recursion)
std::vector<double> doubleRoots(std::span<const double> coefficients, double lower, double upper) {
    std::size_t degree = coefficients.size();
    while (degree > 0 && coefficients[degree - 1] == 0.0) {
        --degree;
    }
    std::vector<double> roots;
    if (degree <= 1) {
        return roots;
    }
    const auto polynomial = coefficients.first(degree);
    std::vector<double> derivative(degree - 1);
    for (std::size_t i = 1; i < degree; ++i) {
        derivative[i - 1] = static_cast<double>(i) * polynomial[i];
    }
    std::vector<double> breaks{lower};
    for (const double critical : doubleRoots(derivative, lower, upper)) {
        breaks.push_back(critical);
    }
    breaks.push_back(upper);
    for (std::size_t i = 0; i + 1 < breaks.size(); ++i) {
        const double atLow = evaluate(polynomial, breaks[i]);
        const double atHigh = evaluate(polynomial, breaks[i + 1]);
        if (atLow == 0.0) {
            roots.push_back(breaks[i]);
        } else if (atHigh != 0.0 && (atLow < 0.0) != (atHigh < 0.0)) {
            roots.push_back(bisect(polynomial, breaks[i], breaks[i + 1], atLow));
        }
    }
    if (evaluate(polynomial, upper) == 0.0) {
        roots.push_back(upper);
    }
    return roots;
}

// A root of the exact polynomial near `guess`, between two adjacent doubles
// (or a double that is a root): the closed interval of doubles that holds it,
// or nothing. The double guess comes from rounded coefficients, so it may be
// some ulp off: the search widens around it until the exact signs differ,
// then bisects over the doubles with exact signs. A sign change proves a
// root in between; the interval may hold an odd number of them, which the
// count of the caller excludes.
std::optional<std::pair<double, double>> certify(const Polynomial& exactPolynomial, double guess) {
    const auto signAt = [&exactPolynomial](double x) {
        return exactPolynomial.signAt(Dyadic{x});
    };
    const auto atGuess = signAt(guess);
    if (atGuess == Sign::zero) {
        return std::pair{guess, guess};
    }
    // Steps of 4^j times the spacing of the doubles at the guess (at least
    // 2^-64, as the roots lie in [-1, 1]), up to 2^38 of them.
    constexpr int kWideningSteps = 20;
    constexpr double kSmallestStep = 0x1p-64;
    const double infinity = std::numeric_limits<double>::infinity();
    const double spacing =
        std::max(std::nextafter(std::fabs(guess), infinity) - std::fabs(guess), kSmallestStep);
    double low = guess;
    double high = guess;
    auto atLow = atGuess;
    bool found = false;
    for (int j = 0; j < kWideningSteps && !found; ++j) {
        const double step = std::ldexp(spacing, 2 * j);
        low = guess - step;
        high = guess + step;
        atLow = signAt(low);
        if (atLow == Sign::zero) {
            return std::pair{low, low};
        }
        const auto atHigh = signAt(high);
        if (atHigh == Sign::zero) {
            return std::pair{high, high};
        }
        if (atLow != atGuess) {
            high = guess;
            found = true;
        } else if (atHigh != atGuess) {
            low = guess;
            atLow = atGuess;
            found = true;
        }
    }
    if (!found) {
        return std::nullopt;
    }
    for (;;) {
        const double middle = 0.5 * low + 0.5 * high;
        if (middle <= low || middle >= high) {
            return std::pair{low, high};
        }
        const auto atMiddle = signAt(middle);
        if (atMiddle == Sign::zero) {
            return std::pair{middle, middle};
        }
        if (atMiddle == atLow) {
            low = middle;
        } else {
            high = middle;
        }
    }
}

// The certified roots of an exact polynomial in a closed interval of
// doubles, in increasing order, each a double within one ulp of a distinct
// exact root (the brackets do not overlap); nothing when one root found
// cannot be certified.
std::optional<std::vector<double>> certifiedRoots(const Polynomial& exactPolynomial,
                                                  const Coefficients& coefficients, double lower,
                                                  double upper) {
    std::vector<double> roots;
    double lastUpper = -std::numeric_limits<double>::infinity();
    for (const double guess : doubleRoots(coefficients, lower, upper)) {
        const auto bracket = certify(exactPolynomial, guess);
        if (!bracket || !(bracket->first > lastUpper) || bracket->first < lower ||
            bracket->second > upper) {
            return std::nullopt;
        }
        lastUpper = bracket->second;
        // Within one ulp of the root, or the root itself.
        roots.push_back(bracket->first);
    }
    return roots;
}

std::optional<Coefficients> roundedCoefficients(const Polynomial& exactPolynomial) {
    Coefficients coefficients{};
    for (std::size_t i = 0; i < coefficients.size(); ++i) {
        coefficients.at(i) = exactPolynomial.coefficient(i).toDouble();
        if (!std::isfinite(coefficients.at(i))) {
            return std::nullopt;
        }
    }
    return coefficients;
}

// The feet by the filter, or nothing when it cannot prove that it has found
// every one of them (see the top of the file). Preconditions: Q != 0, E != 0,
// `expected` the number of feet E gives.
std::optional<std::vector<Foot>> filteredFeet(const Chooser& chooser, const Polynomial& g,
                                              const Polynomial& h, std::size_t expected) {
    const auto gDoubles = roundedCoefficients(g);
    const auto hDoubles = roundedCoefficients(h);
    if (!gDoubles || !hDoubles) {
        return std::nullopt;
    }
    // u in [-1, 1]; v strictly inside (-1, 1), so that no foot is counted
    // twice (|u| <= 1 and |u| > 1 are apart).
    const auto inU = certifiedRoots(g, *gDoubles, -1.0, 1.0);
    if (!inU) {
        return std::nullopt;
    }
    const double justInside = std::nextafter(1.0, 0.0);
    const auto inV = certifiedRoots(h, *hDoubles, -justInside, justInside);
    if (!inV || inU->size() + inV->size() != expected) {
        return std::nullopt;
    }
    std::vector<Foot> feet;
    for (const double u : *inU) {
        feet.push_back(chooser.foot(unitPointAtU(u)));
    }
    for (const double v : *inV) {
        feet.push_back(chooser.foot(unitPointAtV(v)));
    }
    return feet;
}

}  // namespace

Result<EllipseNearest> nearest(const Ellipse& ellipse, Point2 p) {
    const auto meaning = detail::checkedEllipse(ellipse, 0);
    if (!meaning.hasValue()) {
        return meaning.diagnosis();
    }
    if (const auto diagnosis = checkPoint(p)) {
        return *diagnosis;
    }
    const auto& support = meaning.value().support;
    const auto& arc = meaning.value().unitArc;
    const auto ends = endpoints(ellipse);
    assert(ends.hasValue());

    Chooser chooser(ellipse, p);
    if (arc.span == detail::ArcSpan::partial) {
        chooser.offer(ends.value().start, Location::atStart);
        chooser.offer(ends.value().end, Location::atEnd);
    }

    const auto exactP = detail::exactVector(p);
    const ExactVector q{exactP.x - support.center.x, exactP.y - support.center.y};
    const auto bigP = dot(q, support.major);
    const auto bigQ = dot(q, support.minor);
    const auto gamma = dot(support.major, support.major) - dot(support.minor, support.minor);
    const Polynomial g({-bigQ, kTwo * (bigP - gamma), Dyadic{}, kTwo * (bigP + gamma), bigQ});

    // The filter: simple roots, none at infinity.
    if (!bigQ.isZero()) {
        const auto squares = bigP * bigP + bigQ * bigQ - gamma * gamma;
        const auto evolute = squares * squares * squares +
                             Dyadic::fromInteger(27) * bigP * bigP * bigQ * bigQ * gamma * gamma;
        if (!evolute.isZero()) {
            const std::size_t expected = evolute.sign() == Sign::negative ? 4 : 2;
            const Polynomial h(
                {bigQ, kTwo * (bigP + gamma), Dyadic{}, kTwo * (bigP - gamma), -bigQ});
            if (auto feet = filteredFeet(chooser, g, h, expected)) {
                Chooser filtered = chooser;
                if (offerFeet(filtered, arc, std::move(*feet),
                              [&arc](const Foot& foot) { return locateUnit(arc, foot.unit); })) {
                    return filtered.best();
                }
            }
        }
    }

    // The exact path.
    if (g.isZero()) {
        // A circle seen from its centre: every point is a foot at the same
        // distance. The start of the curve is one of them.
        chooser.offer(ends.value().start,
                      arc.span == detail::ArcSpan::partial ? Location::atStart : Location::inside);
        return chooser.best();
    }
    std::vector<Foot> feet;
    for (auto& root : exact::realRoots(g)) {
        const auto unit = unitPointAt(exact::toDouble(root));
        feet.push_back(chooser.foot(unit, std::move(root)));
    }
    if (bigQ.isZero()) {
        feet.push_back(chooser.foot({-1.0, 0.0}));
    }
    [[maybe_unused]] const bool located =
        offerFeet(chooser, arc, std::move(feet), [&arc](const Foot& foot) {
            if (foot.root.has_value()) {
                return std::optional<Location>{locateRoot(arc, foot.root.value())};
            }
            const ExactVector opposite{Dyadic(-1.0), Dyadic{}};
            return std::optional<Location>{detail::locateDirection(arc, opposite)};
        });
    assert(located);
    return chooser.best();
}

}  // namespace solid_space::geom2d
