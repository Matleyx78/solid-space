// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Real roots of polynomials of degree at most four (polynomial.hpp; stage 0.3,
// package PS3).
//
// Everything is exact dyadic arithmetic: the only rounding is the final one of
// toDouble(). The remainder sequences use pseudo-remainders, scaled by a
// positive factor so that the signs Sturm's theorem and the Tarski query look
// at are the true ones (a positive multiple of a term has the same signs, and
// the later remainders are positive multiples of the true ones).
//
// The signs are filtered first (stage 0.3 corrections, package FS4; section
// "The floating-point filter" below): the filter decides a sign only when it
// is certain, and otherwise the exact computation runs, so every answer and
// every interval is the one the exact computation alone gives.

#include "polynomial.hpp"

#include "interval.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <optional>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace solid_space::geom2d::exact {
namespace {

using robust::Sign;

// toDouble() stops bisecting when the interval is at most 2^-kRoundingBits
// times the smaller of its ends in absolute value: the midpoint is then within
// 2^-61 |root| of the root, far below half an ulp (2^-54 |root| at least), so
// the rounding of the midpoint is the rounding of the root up to that margin.
constexpr std::int64_t kRoundingBits = 60;
// ... or when the interval is at most 2^kAbsoluteWidthExponent wide, for the
// roots near zero: the midpoint is within 2^-1081 of the root, far below half
// the smallest subnormal (2^-1075).
constexpr std::int64_t kAbsoluteWidthExponent = -1080;

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

// ---------------------------------------------------------------------------
// The floating-point filter (stage 0.3 corrections, package FS4)
//
// Every decision here is the sign of a polynomial with dyadic coefficients:
// at a dyadic point, or at a root. The filter computes it first with the
// intervals of interval.hpp. Each operation there gives an interval of the
// exact result of the same operation on any exact operands within the
// intervals of its operands (the proofs are next to each operation, in
// interval.cpp), and intervalOf() rounds a dyadic number with the bound of its
// rounding. So Horner's rule written with those operations is Horner's rule
// with a running error bound: by induction on its steps, its last interval
// contains p(X) for every exact X within the interval of the point, with the
// exact coefficients of p. certainSign() answers only when that interval lies
// on one side of zero: an answer of the filter is the exact sign, and never
// zero. When the filter does not answer, the exact computation runs as
// before. The filter is on unless the tests turn it off (filterEnabled()), and
// every filtered sign is counted for them (recordFilterAnswer()).

constexpr double kInfinity = std::numeric_limits<double>::infinity();

// The coefficients of a polynomial of degree at most kMaxDegree, as
// intervals, for the signs at many points.
class IntervalPolynomial {
public:
    explicit IntervalPolynomial(std::span<const Dyadic> coefficients) noexcept {
        assert(coefficients.size() <= m_values.size());
        for (const Dyadic& coefficient : coefficients) {
            m_values.at(m_size) = intervalOf(coefficient);
            ++m_size;
        }
    }

    // An interval of p(X) for every exact X within x (Horner's rule, above).
    [[nodiscard]] Interval valueAt(const Interval& x) const noexcept {
        Interval value = intervalOf(0.0);
        for (std::size_t i = m_size; i-- > 0;) {
            value = (value * x) + m_values.at(i);
        }
        return value;
    }

private:
    std::array<Interval, static_cast<std::size_t>(kMaxDegree) + 1> m_values{};
    std::size_t m_size = 0;
};

// The same for a polynomial of any degree, its coefficients rounded on the way.
Interval valueInterval(std::span<const Dyadic> coefficients, const Interval& x) noexcept {
    Interval value = intervalOf(0.0);
    for (const Dyadic& coefficient : std::views::reverse(coefficients)) {
        value = (value * x) + intervalOf(coefficient);
    }
    return value;
}

// The sign of the exact number of `value` when the filter decides it; counted.
std::optional<Sign> decided(const Interval& value) noexcept {
    const auto sign = certainSign(value);
    recordFilterAnswer(sign.has_value());
    return sign;
}

// An interval of every X with L <= X <= U, from intervals a of L and b of U.
// M = (L + U) / 2 is within middle and H = (U - L) / 2 >= 0 within reach, so
// |X - M| <= H <= |reach.centre| + reach.radius: X = M + Y1 + Y2 with
// |Y1| <= |reach.centre| and |Y2| <= reach.radius (Y1 and Y2 the parts of
// X - M in proportion to the two bounds), and the sums of intervals contain it.
Interval spanOf(const Interval& a, const Interval& b) noexcept {
    const Interval half = intervalOf(0.5);
    const Interval middle = (a + b) * half;
    const Interval reach = (b - a) * half;
    return (middle + Interval{0.0, std::fabs(reach.centre)}) + Interval{0.0, reach.radius};
}

// The exact sign of p at x: filtered with the intervals of its coefficients,
// exact otherwise. `point` is intervalOf(x).
Sign filteredSign(const Polynomial& p, const IntervalPolynomial& intervals, const Dyadic& x,
                  const Interval& point) {
    if (filterEnabled()) {
        if (const auto sign = decided(intervals.valueAt(point))) {
            return *sign;
        }
    }
    return p.evaluate(x).sign();
}

// The coefficients of p(c + h) as a polynomial in h, exact: Taylor's shift by
// repeated synthetic division (the k-th pass leaves the k-th coefficient).
std::vector<Dyadic> shiftedCoefficients(const Polynomial& p, const Dyadic& c) {
    std::vector<Dyadic> shifted(p.coefficients().begin(), p.coefficients().end());
    const std::size_t size = shifted.size();
    for (std::size_t k = 0; k + 1 < size; ++k) {
        for (std::size_t j = size - 1; j-- > k;) {
            shifted[j] += c * shifted[j + 1];
        }
    }
    return shifted;
}

Dyadic powerOfTwo(std::int64_t exponent) {
    return Dyadic(1.0).timesPowerOfTwo(exponent);
}

Dyadic midpoint(const Dyadic& a, const Dyadic& b) {
    return (a + b).timesPowerOfTwo(-1);
}

// The largest e with 2^e <= |x|. Precondition: x is not zero. A gallop then a
// bisection on the exponent: about 2 log2(|e|) comparisons, each without
// allocation for the powers of two.
std::int64_t floorLog2(const Dyadic& x) {
    assert(!x.isZero());
    const Dyadic magnitude = x.abs();
    std::int64_t lower = 0;
    std::int64_t upper = 0;
    std::int64_t step = 1;
    if (compare(magnitude, powerOfTwo(0)) != Sign::negative) {
        while (compare(magnitude, powerOfTwo(lower + step)) != Sign::negative) {
            lower += step;
            step *= 2;
        }
        upper = lower + step;
    } else {
        while (compare(magnitude, powerOfTwo(upper - step)) == Sign::negative) {
            upper -= step;
            step *= 2;
        }
        lower = upper - step;
    }
    // 2^lower <= |x| < 2^upper.
    while (upper - lower > 1) {
        const std::int64_t middle = lower + ((upper - lower) / 2);
        if (compare(magnitude, powerOfTwo(middle)) != Sign::negative) {
            lower = middle;
        } else {
            upper = middle;
        }
    }
    return lower;
}

// The quotient rounded up, for a positive divisor.
std::int64_t ceilDivide(std::int64_t numerator, std::int64_t divisor) {
    assert(divisor > 0);
    std::int64_t quotient = numerator / divisor;  // rounds toward zero
    if (numerator > 0 && numerator % divisor != 0) {
        ++quotient;
    }
    return quotient;
}

std::size_t sizeOf(const Polynomial& p) {
    return p.coefficients().size();
}

struct PseudoDivision {
    Polynomial quotient;
    Polynomial remainder;
};

// With d the leading coefficient of b and k = deg a - deg b + 1 >= 1:
// d^k * a = quotient * b + remainder, with deg remainder < deg b. Every step
// multiplies by d, even when the coefficient to cancel is already zero, so the
// power is exactly k. Preconditions: b is not zero, deg a >= deg b.
PseudoDivision pseudoDivide(const Polynomial& a, const Polynomial& b) {
    assert(!b.isZero() && a.degree() >= b.degree());
    const std::size_t divisorSize = sizeOf(b);
    const std::size_t shifts = sizeOf(a) - divisorSize + 1;
    const std::span<const Dyadic> divisor = b.coefficients();
    const Dyadic& lead = divisor.back();
    std::vector<Dyadic> remainder(a.coefficients().begin(), a.coefficients().end());
    std::vector<Dyadic> quotient(shifts);
    for (std::size_t step = 0; step < shifts; ++step) {
        const std::size_t shift = shifts - 1 - step;
        const std::size_t top = shift + divisorSize - 1;
        const Dyadic factor = remainder[top];
        for (Dyadic& coefficient : quotient) {
            coefficient *= lead;
        }
        quotient[shift] += factor;
        for (std::size_t i = 0; i < top; ++i) {
            remainder[i] *= lead;
        }
        if (!factor.isZero()) {
            for (std::size_t j = 0; j + 1 < divisorSize; ++j) {
                remainder[shift + j] -= factor * divisor[j];
            }
        }
        // lead * factor - factor * lead: cancelled exactly.
        remainder[top] = Dyadic();
    }
    remainder.resize(divisorSize - 1);
    return {Polynomial(std::move(quotient)), Polynomial(std::move(remainder))};
}

// A positive multiple of the remainder of a divided by b: |d|^k times it, with
// d and k as in pseudoDivide; a itself when deg a < deg b. Precondition: b is
// not zero.
Polynomial signedPseudoRemainder(const Polynomial& a, const Polynomial& b) {
    assert(!b.isZero());
    if (a.degree() < b.degree()) {
        return a;
    }
    PseudoDivision division = pseudoDivide(a, b);
    const auto power = static_cast<std::size_t>(a.degree() - b.degree()) + 1;
    if (b.leadingCoefficient().sign() == Sign::negative && power % 2 == 1) {
        return -division.remainder;
    }
    return std::move(division.remainder);
}

// The signed remainder sequence of a and b: a, b, then -rem of the two before,
// to the last nonzero term; every term a positive multiple of the true one.
// Precondition: a is not zero.
std::vector<Polynomial> signedRemainderSequence(const Polynomial& a, const Polynomial& b) {
    assert(!a.isZero());
    std::vector<Polynomial> terms{a};
    if (b.isZero()) {
        return terms;
    }
    terms.push_back(b);
    while (true) {
        Polynomial next = -signedPseudoRemainder(terms[terms.size() - 2], terms.back());
        if (next.isZero()) {
            break;
        }
        terms.push_back(std::move(next));
    }
    return terms;
}

// Sign changes of the values in `signs`, zeros skipped.
std::size_t signChanges(const std::vector<Sign>& signs) {
    std::size_t changes = 0;
    Sign previous = Sign::zero;
    for (const Sign sign : signs) {
        if (sign == Sign::zero) {
            continue;
        }
        if (previous != Sign::zero && sign != previous) {
            ++changes;
        }
        previous = sign;
    }
    return changes;
}

std::size_t signChangesAt(const std::vector<Polynomial>& terms, const Dyadic& x) {
    std::vector<Sign> signs;
    signs.reserve(terms.size());
    for (const Polynomial& term : terms) {
        signs.push_back(term.signAt(x));
    }
    return signChanges(signs);
}

// At +infinity each term has the sign of its leading coefficient; at
// -infinity, that sign changed for the terms of odd degree.
std::size_t signChangesAtInfinity(const std::vector<Polynomial>& terms, bool positive) {
    std::vector<Sign> signs;
    signs.reserve(terms.size());
    for (const Polynomial& term : terms) {
        const Sign lead = term.leadingCoefficient().sign();
        const bool odd = term.degree() % 2 != 0;
        signs.push_back(positive || !odd ? lead : negated(lead));
    }
    return signChanges(signs);
}

// The Sturm sequence of the squarefree polynomial s: s, s', and the negated
// remainders, the last one a nonzero constant.
std::vector<Polynomial> sturmTerms(const Polynomial& s) {
    return signedRemainderSequence(s, s.derivative());
}

// A Sturm sequence with the intervals of the coefficients of its terms,
// computed once for all the points where realRoots() counts its sign changes.
class FilteredSequence {
public:
    explicit FilteredSequence(std::vector<Polynomial> terms) : m_terms(std::move(terms)) {
        m_intervals.reserve(m_terms.size());
        for (const Polynomial& term : m_terms) {
            m_intervals.emplace_back(term.coefficients());
        }
    }

    // The sign of the first term at x, exact.
    [[nodiscard]] Sign frontSignAt(const Dyadic& x) const {
        return filteredSign(m_terms.front(), m_intervals.front(), x, intervalOf(x));
    }

    // The sign changes at x, as signChangesAt().
    [[nodiscard]] std::size_t changesAt(const Dyadic& x) const {
        const Interval point = intervalOf(x);
        std::vector<Sign> signs;
        signs.reserve(m_terms.size());
        for (std::size_t i = 0; i < m_terms.size(); ++i) {
            signs.push_back(filteredSign(m_terms[i], m_intervals[i], x, point));
        }
        return signChanges(signs);
    }

private:
    std::vector<Polynomial> m_terms;
    std::vector<IntervalPolynomial> m_intervals;
};

// ---------------------------------------------------------------------------
// The bracket of a root (RealRoot::m_below and m_above)
//
// Within the isolating interval (lower, upper) of a root r, the squarefree
// part s has the sign of lower (signAboveLower()) below r, the opposite one
// above r, and no zero but r. So a double t is below r when t <= lower, or
// when lower < t < upper and the filter decides that s has the sign of lower
// at t; it is above r when t >= upper, or when lower < t < upper and the
// filter decides the opposite sign. Two doubles a < b checked so are a
// bracket of r: a < r < b. They are searched for in plain floating point
// (Newton's method, kept within a pair of doubles that bisection shrinks),
// whose errors do not matter: only the checks make a and b a bracket, and
// when a check fails the root has none, and the exact paths run.

struct Bracket {
    double below = -kInfinity;
    double above = kInfinity;
};

// At most this many steps of Newton's method or bisection in floating point.
constexpr int kBracketSteps = 64;

// The first half-width tried around the approximation x: 2^-44 times the
// size of the terms of s at x over its slope, so that s at x +- reach is
// about 2^9 times its rounding error ((2 degree + 1) 2^-53 times that size at
// most, and the radius of the filter is of the same order); 2^-50 |x| more
// for a root where the size of the terms is zero. Each failed check widens it
// 2^8 times, at most kBracketAttempts times.
constexpr double kReachFactor = 0x1p-44;
constexpr double kRelativeReach = 0x1p-50;
constexpr double kWidening = 0x1p8;
constexpr int kBracketAttempts = 3;

// Where the double t lies with respect to the root isolated in
// (lower, upper), by the checks above: negative below it, positive above it,
// zero when the checks do not decide. Precondition: t is finite.
Sign sideOf(double t, const RealRoot& root, const IntervalPolynomial& squarefree) {
    const Dyadic value(t);
    if (compare(value, root.lower()) != Sign::positive) {
        return Sign::negative;
    }
    if (compare(value, root.upper()) != Sign::negative) {
        return Sign::positive;
    }
    const auto sign = certainSign(squarefree.valueAt(intervalOf(t)));
    if (!sign) {
        return Sign::zero;
    }
    return *sign == root.signAboveLower() ? Sign::negative : Sign::positive;
}

// The value of s at x in floating point, its derivative, and the size of its
// terms (the sum of |c_i| |x|^i). Only estimates: nothing is decided on them.
struct Estimate {
    double value = 0.0;
    double slope = 0.0;
    double size = 0.0;
};

Estimate estimate(std::span<const double> coefficients, double x) noexcept {
    Estimate result;
    const double magnitude = std::fabs(x);
    for (const double coefficient : std::views::reverse(coefficients)) {
        result.slope = (result.slope * x) + result.value;
        result.value = (result.value * x) + coefficient;
        result.size = (result.size * magnitude) + std::fabs(coefficient);
    }
    return result;
}

// The bracket of a root that is not exact, or none (see above). Only with the
// filter on: with it off the exact paths run alone.
Bracket findBracket(const RealRoot& root) {
    if (root.isExact() || !filterEnabled()) {
        return {};
    }
    const std::span<const Dyadic> exactCoefficients = root.squarefree().coefficients();
    std::array<double, static_cast<std::size_t>(kMaxDegree) + 1> values{};
    for (std::size_t i = 0; i < exactCoefficients.size(); ++i) {
        values.at(i) = exactCoefficients[i].toDouble();
        if (!std::isfinite(values.at(i))) {
            return {};
        }
    }
    const std::span<const double> coefficients(values.data(), exactCoefficients.size());
    double low = root.lower().toDouble();
    double high = root.upper().toDouble();
    if (!std::isfinite(low) || !std::isfinite(high)) {
        return {};
    }
    const double lowerSide = root.signAboveLower() == Sign::positive ? 1.0 : -1.0;
    double x = (0.5 * low) + (0.5 * high);
    for (int step = 0; step < kBracketSteps; ++step) {
        const Estimate at = estimate(coefficients, x);
        if (at.value == 0.0) {
            break;
        }
        if (at.value * lowerSide > 0.0) {
            low = x;
        } else {
            high = x;
        }
        double next = x - (at.value / at.slope);
        // Bisection when the step leaves the pair, or is not a number.
        if (!std::isfinite(next) || next <= low || next >= high) {
            next = (0.5 * low) + (0.5 * high);
        }
        if (next == x) {
            break;
        }
        x = next;
    }
    const Estimate at = estimate(coefficients, x);
    double reach =
        (kReachFactor * (at.size / std::fabs(at.slope))) + (kRelativeReach * std::fabs(x));
    if (!std::isfinite(reach)) {
        return {};
    }
    const IntervalPolynomial squarefree(exactCoefficients);
    for (int attempt = 0; attempt < kBracketAttempts; ++attempt) {
        const double below = x - reach;
        const double above = x + reach;
        if (!std::isfinite(below) || !std::isfinite(above)) {
            break;
        }
        if (sideOf(below, root, squarefree) == Sign::negative &&
            sideOf(above, root, squarefree) == Sign::positive) {
            return {below, above};
        }
        reach *= kWidening;
    }
    return {};
}

// The sign of the squarefree part s of a root at a dyadic x strictly within
// its isolating interval, as the bisections of narrowed() and toDouble() need
// it: exact, the same as s.signAt(x).
// - At or below the lower end of the bracket x is below the root, where s has
//   the sign of the lower end of the interval; at or above its upper end, the
//   opposite sign (see "The bracket of a root").
// - Otherwise the filter on s at x; then the filter on the shifted polynomial
//   s(c + h) = t0 + t1 h + ... at h = x - c, exact, for a double c near the
//   root. Near the root the terms of s nearly cancel, and the rounding of
//   s(x) in floating point is too large to decide; the shifted coefficients
//   carry that cancellation exactly (t0 = s(c) is small, h is small), so the
//   rounding left is far smaller. s(x) is exactly the shifted polynomial at
//   x - c, so the filter of the section above applies to it unchanged.
// - Otherwise the exact value.
class BisectionSigns {
public:
    BisectionSigns(const RealRoot& root, double below, double above)
        : m_root(root), m_values(root.squarefree().coefficients()) {
        if (std::isfinite(below) && std::isfinite(above)) {
            m_bracket = {Dyadic(below), Dyadic(above)};
        }
    }

    [[nodiscard]] Sign at(const Dyadic& x) {
        if (filterEnabled()) {
            if (const auto sign = filtered(x)) {
                recordFilterAnswer(true);
                return *sign;
            }
            recordFilterAnswer(false);
        }
        return m_root.squarefree().evaluate(x).sign();
    }

private:
    struct Shifted {
        Dyadic centre;
        IntervalPolynomial values;
    };

    [[nodiscard]] std::optional<Sign> filtered(const Dyadic& x) {
        if (m_bracket) {
            if (compare(x, m_bracket->first) != Sign::positive) {
                return m_root.signAboveLower();
            }
            if (compare(x, m_bracket->second) != Sign::negative) {
                return negated(m_root.signAboveLower());
            }
        }
        const Interval point = intervalOf(x);
        if (const auto sign = certainSign(m_values.valueAt(point))) {
            return sign;
        }
        if (!isKnown(point)) {
            return std::nullopt;
        }
        if (!m_shifted) {
            Dyadic centre(point.centre);
            const IntervalPolynomial values(shiftedCoefficients(m_root.squarefree(), centre));
            m_shifted.emplace(Shifted{std::move(centre), values});
        }
        return certainSign(m_shifted->values.valueAt(intervalOf(x - m_shifted->centre)));
    }

    const RealRoot& m_root;
    IntervalPolynomial m_values;
    std::optional<std::pair<Dyadic, Dyadic>> m_bracket;
    std::optional<Shifted> m_shifted;
};

// A power of two B with every complex root z of p in |z| < B. Fujiwara's bound
// says |z| <= 2 * max over i = 1..n of |a_{n-i} / a_n|^(1/i) (the last term
// even halved); with k the smallest integer such that |a_{n-i}| < |a_n| *
// 2^(k i) for every i, the maximum is below 2^k, so 2^(k + 1) is strictly
// larger than every |z|. The exponents of the coefficients are those of
// floorLog2: |a_{n-i}| < 2^(f_i + 1) and |a_n| >= 2^(f_n). Precondition: the
// degree of p is at least one.
Dyadic rootBound(const Polynomial& p) {
    assert(p.degree() >= 1);
    const auto degree = static_cast<std::size_t>(p.degree());
    const std::int64_t leadExponent = floorLog2(p.leadingCoefficient());
    std::optional<std::int64_t> exponent;
    for (std::size_t i = 1; i <= degree; ++i) {
        const Dyadic coefficient = p.coefficient(degree - i);
        if (coefficient.isZero()) {
            continue;
        }
        const std::int64_t candidate =
            ceilDivide(floorLog2(coefficient) + 1 - leadExponent, static_cast<std::int64_t>(i));
        exponent = std::max(exponent.value_or(candidate), candidate);
    }
    // Only a_n x^n: the only root is zero.
    return powerOfTwo(exponent.value_or(0) + 1);
}

// The multiplicity of a root of p: one more than the number of derivatives
// of p, in order, that vanish there.
int multiplicityOf(const Polynomial& p, const RealRoot& root) {
    int multiplicity = 1;
    Polynomial derivative = p.derivative();
    while (!derivative.isZero() && signAt(derivative, root) == Sign::zero) {
        ++multiplicity;
        derivative = derivative.derivative();
    }
    return multiplicity;
}

}  // namespace

// ---------------------------------------------------------------------------
// Polynomial

Polynomial::Polynomial(std::vector<Dyadic> coefficients) : m_coefficients(std::move(coefficients)) {
    trim();
}

Polynomial::Polynomial(std::initializer_list<Dyadic> coefficients) : m_coefficients(coefficients) {
    trim();
}

void Polynomial::trim() {
    while (!m_coefficients.empty() && m_coefficients.back().isZero()) {
        m_coefficients.pop_back();
    }
}

int Polynomial::degree() const noexcept {
    return static_cast<int>(m_coefficients.size()) - 1;
}

Dyadic Polynomial::coefficient(std::size_t power) const {
    return power < m_coefficients.size() ? m_coefficients[power] : Dyadic();
}

Dyadic Polynomial::leadingCoefficient() const {
    return m_coefficients.empty() ? Dyadic() : m_coefficients.back();
}

Polynomial Polynomial::derivative() const {
    std::vector<Dyadic> result;
    if (m_coefficients.size() > 1) {
        result.reserve(m_coefficients.size() - 1);
        for (std::size_t i = 1; i < m_coefficients.size(); ++i) {
            result.push_back(Dyadic::fromInteger(static_cast<std::int64_t>(i)) * m_coefficients[i]);
        }
    }
    return Polynomial(std::move(result));
}

Dyadic Polynomial::evaluate(const Dyadic& x) const {
    Dyadic value;
    for (const Dyadic& coefficient : std::views::reverse(m_coefficients)) {
        value = (value * x) + coefficient;
    }
    return value;
}

robust::Sign Polynomial::signAt(const Dyadic& x) const {
    if (filterEnabled()) {
        if (const auto sign = decided(valueInterval(m_coefficients, intervalOf(x)))) {
            return *sign;
        }
    }
    return evaluate(x).sign();
}

Polynomial operator+(const Polynomial& a, const Polynomial& b) {
    std::vector<Dyadic> result(std::max(a.m_coefficients.size(), b.m_coefficients.size()));
    for (std::size_t i = 0; i < result.size(); ++i) {
        result[i] = a.coefficient(i) + b.coefficient(i);
    }
    return Polynomial(std::move(result));
}

Polynomial operator-(const Polynomial& a, const Polynomial& b) {
    std::vector<Dyadic> result(std::max(a.m_coefficients.size(), b.m_coefficients.size()));
    for (std::size_t i = 0; i < result.size(); ++i) {
        result[i] = a.coefficient(i) - b.coefficient(i);
    }
    return Polynomial(std::move(result));
}

Polynomial operator-(const Polynomial& a) {
    std::vector<Dyadic> result;
    result.reserve(a.m_coefficients.size());
    for (const Dyadic& coefficient : a.m_coefficients) {
        result.push_back(-coefficient);
    }
    return Polynomial(std::move(result));
}

Polynomial operator*(const Polynomial& a, const Polynomial& b) {
    if (a.isZero() || b.isZero()) {
        return {};
    }
    std::vector<Dyadic> result(a.m_coefficients.size() + b.m_coefficients.size() - 1);
    for (std::size_t i = 0; i < a.m_coefficients.size(); ++i) {
        if (a.m_coefficients[i].isZero()) {
            continue;
        }
        for (std::size_t j = 0; j < b.m_coefficients.size(); ++j) {
            result[i + j] += a.m_coefficients[i] * b.m_coefficients[j];
        }
    }
    return Polynomial(std::move(result));
}

Polynomial operator*(const Polynomial& a, const Dyadic& factor) {
    std::vector<Dyadic> result;
    result.reserve(a.m_coefficients.size());
    for (const Dyadic& coefficient : a.m_coefficients) {
        result.push_back(coefficient * factor);
    }
    return Polynomial(std::move(result));
}

Polynomial operator*(const Dyadic& factor, const Polynomial& a) {
    return a * factor;
}

bool operator==(const Polynomial& a, const Polynomial& b) {
    return std::equal(a.m_coefficients.begin(), a.m_coefficients.end(), b.m_coefficients.begin(),
                      b.m_coefficients.end(),
                      [](const Dyadic& x, const Dyadic& y) { return compare(x, y) == Sign::zero; });
}

// ---------------------------------------------------------------------------
// Divisors

Polynomial greatestCommonDivisor(const Polynomial& a, const Polynomial& b) {
    assert(a.degree() <= kMaxDegree && b.degree() <= kMaxDegree);
    Polynomial larger = a.degree() >= b.degree() ? a : b;
    Polynomial smaller = a.degree() >= b.degree() ? b : a;
    while (!smaller.isZero()) {
        Polynomial remainder = signedPseudoRemainder(larger, smaller);
        larger = std::move(smaller);
        smaller = std::move(remainder);
    }
    return larger;
}

Polynomial squarefreePart(const Polynomial& p) {
    assert(!p.isZero() && p.degree() <= kMaxDegree);
    if (p.degree() < 1) {
        return p;
    }
    const Polynomial divisor = greatestCommonDivisor(p, p.derivative());
    if (divisor.degree() < 1) {
        return p;
    }
    // The remainder is zero: the divisor divides p.
    return pseudoDivide(p, divisor).quotient;
}

// ---------------------------------------------------------------------------
// Sturm sequences

SturmSequence::SturmSequence(const Polynomial& p) {
    assert(!p.isZero() && p.degree() <= kMaxDegree);
    const Polynomial squarefree = squarefreePart(p);
    if (squarefree.degree() < 1) {
        m_terms.push_back(squarefree);
    } else {
        m_terms = sturmTerms(squarefree);
    }
}

std::size_t SturmSequence::countRoots() const {
    return signChangesAtInfinity(m_terms, false) - signChangesAtInfinity(m_terms, true);
}

std::size_t SturmSequence::countRoots(const Dyadic& lower, const Dyadic& upper) const {
    assert(compare(lower, upper) != Sign::positive);
    // V(lower) - V(upper) counts the roots in (lower, upper]; lower is added
    // when it is a root.
    const std::size_t aboveLower = signChangesAt(m_terms, lower) - signChangesAt(m_terms, upper);
    return aboveLower + (m_terms.front().signAt(lower) == Sign::zero ? 1 : 0);
}

// ---------------------------------------------------------------------------
// Roots

RealRoot::RealRoot(Polynomial squarefree, Dyadic lower, Dyadic upper, int multiplicity)
    : m_squarefree(std::move(squarefree)),
      m_lower(std::move(lower)),
      m_upper(std::move(upper)),
      m_multiplicity(multiplicity),
      m_exact(compare(m_lower, m_upper) == Sign::zero) {
    assert(compare(m_lower, m_upper) != Sign::positive);
    m_signAboveLower = m_exact ? Sign::zero : m_squarefree.signAt(m_lower);
    assert(m_exact || m_signAboveLower != Sign::zero);
}

std::vector<RealRoot> realRoots(const Polynomial& p) {
    assert(!p.isZero() && p.degree() <= kMaxDegree);
    std::vector<RealRoot> roots;
    if (p.degree() < 1) {
        return roots;
    }
    // The Sturm sequence of p is, up to the signs of its terms, the remainder
    // sequence of greatestCommonDivisor(p, p'), whose last term is that
    // divisor: when it is a constant, p is its own squarefree part
    // (squarefreePart() returns p itself) and the sequence is already the one
    // of the squarefree part, so it is not computed twice.
    std::vector<Polynomial> sequence = sturmTerms(p);
    Polynomial squarefree = p;
    if (sequence.back().degree() > 0) {
        squarefree = squarefreePart(p);
        sequence = sturmTerms(squarefree);
    }
    const FilteredSequence terms(std::move(sequence));

    // An open interval (lower, upper) whose ends are not roots, with the sign
    // changes there: it holds changesLower - changesUpper roots. Or an exact
    // root, when `exact` is set (lower is the root).
    struct Pending {
        Dyadic lower;
        std::size_t changesLower = 0;
        Dyadic upper;
        std::size_t changesUpper = 0;
        bool exact = false;
    };

    const Dyadic bound = rootBound(squarefree);
    std::vector<Pending> stack;
    stack.push_back({-bound, terms.changesAt(-bound), bound, terms.changesAt(bound)});
    // Last in, first out: the lower half is pushed last, so the roots come out
    // in increasing order.
    while (!stack.empty()) {
        const Pending item = std::move(stack.back());
        stack.pop_back();
        if (item.exact) {
            roots.push_back(RealRoot(squarefree, item.lower, item.lower, 1));
            continue;
        }
        const std::size_t count = item.changesLower - item.changesUpper;
        if (count == 0) {
            continue;
        }
        if (count == 1) {
            roots.push_back(RealRoot(squarefree, item.lower, item.upper, 1));
            continue;
        }
        const Dyadic middle = midpoint(item.lower, item.upper);
        if (terms.frontSignAt(middle) != Sign::zero) {
            const std::size_t changesMiddle = terms.changesAt(middle);
            stack.push_back({middle, changesMiddle, item.upper, item.changesUpper});
            stack.push_back({item.lower, item.changesLower, middle, changesMiddle});
            continue;
        }
        // The middle is a root: an exact one, between two intervals whose ends
        // are not roots and that leave no other root out. Halving the gap
        // ends, because the roots are finitely many.
        Dyadic gap = (item.upper - item.lower).timesPowerOfTwo(-2);
        Dyadic below;
        Dyadic above;
        std::size_t changesBelow = 0;
        std::size_t changesAbove = 0;
        while (true) {
            below = middle - gap;
            above = middle + gap;
            if (terms.frontSignAt(below) != Sign::zero && terms.frontSignAt(above) != Sign::zero) {
                changesBelow = terms.changesAt(below);
                changesAbove = terms.changesAt(above);
                if (changesBelow - changesAbove == 1) {
                    break;
                }
            }
            gap = gap.timesPowerOfTwo(-1);
        }
        stack.push_back({above, changesAbove, item.upper, item.changesUpper});
        stack.push_back({middle, 0, middle, 0, true});
        stack.push_back({item.lower, item.changesLower, below, changesBelow});
    }

    for (RealRoot& root : roots) {
        const Bracket bracket = findBracket(root);
        root.m_below = bracket.below;
        root.m_above = bracket.above;
        root.m_multiplicity = multiplicityOf(p, root);
    }
    return roots;
}

robust::Sign signAt(const Polynomial& q, const RealRoot& root) {
    assert(q.degree() <= kMaxDegree);
    if (q.isZero()) {
        return Sign::zero;
    }
    if (root.isExact()) {
        return q.signAt(root.lower());
    }
    // The filter: q on an interval that contains the root, its bracket when it
    // has one (as narrow as floating point finds it), its isolating interval
    // otherwise. The interval of the caller does not change.
    if (filterEnabled()) {
        const Interval enclosure = std::isfinite(root.m_below)
                                       ? spanOf(intervalOf(root.m_below), intervalOf(root.m_above))
                                       : spanOf(intervalOf(root.lower()), intervalOf(root.upper()));
        if (const auto sign = decided(valueInterval(q.coefficients(), enclosure))) {
            return *sign;
        }
    }
    // The Tarski query: with s squarefree, R a positive multiple of s' q
    // modulo s, and (lower, upper) an isolating interval of the root whose
    // ends are not roots of s, the sign changes of the signed remainder
    // sequence of s and R at the two ends differ by the sign of q at the root
    // (the Cauchy index of s' q / s, which equals that of R / s).
    const Polynomial& s = root.squarefree();
    const Polynomial remainder = signedPseudoRemainder(s.derivative() * q, s);
    const std::vector<Polynomial> terms = signedRemainderSequence(s, remainder);
    const std::size_t changesLower = signChangesAt(terms, root.lower());
    const std::size_t changesUpper = signChangesAt(terms, root.upper());
    if (changesLower == changesUpper) {
        return Sign::zero;
    }
    return changesLower > changesUpper ? Sign::positive : Sign::negative;
}

robust::Sign compare(const RealRoot& root, const Dyadic& x) {
    if (root.isExact()) {
        return compare(root.lower(), x);
    }
    if (compare(x, root.lower()) != Sign::positive) {
        return Sign::positive;
    }
    if (compare(x, root.upper()) != Sign::negative) {
        return Sign::negative;
    }
    // x is inside the isolating interval: the squarefree part keeps the sign
    // of its lower end up to the root, and changes it there.
    const Sign value = root.squarefree().signAt(x);
    if (value == Sign::zero) {
        return Sign::zero;
    }
    return value == root.signAboveLower() ? Sign::positive : Sign::negative;
}

robust::Sign compare(const RealRoot& a, const RealRoot& b) {
    if (a.isExact()) {
        return negated(compare(b, a.lower()));
    }
    if (b.isExact()) {
        return compare(a, b.lower());
    }
    if (compare(a.upper(), b.lower()) != Sign::positive) {
        return Sign::negative;
    }
    if (compare(b.upper(), a.lower()) != Sign::positive) {
        return Sign::positive;
    }
    // Where a lies with respect to the isolating interval of b, whose ends
    // are below and above b.
    if (compare(a, b.lower()) != Sign::positive) {
        return Sign::negative;
    }
    if (compare(a, b.upper()) != Sign::negative) {
        return Sign::positive;
    }
    // a is inside the open interval of b, which holds one root of b's
    // squarefree part: b itself when a is a root too, otherwise the side
    // follows from the sign there.
    const Sign valueAtA = signAt(b.squarefree(), a);
    if (valueAtA == Sign::zero) {
        return Sign::zero;
    }
    return valueAtA == b.signAboveLower() ? Sign::negative : Sign::positive;
}

RealRoot narrowed(const RealRoot& root, const Dyadic& width) {
    assert(width.sign() == Sign::positive);
    if (root.isExact()) {
        return root;
    }
    BisectionSigns signs(root, root.m_below, root.m_above);
    Dyadic lower = root.lower();
    Dyadic upper = root.upper();
    // upper - lower, halved exactly at each step.
    Dyadic span = upper - lower;
    while (compare(span, width) == Sign::positive) {
        Dyadic middle = midpoint(lower, upper);
        const Sign value = signs.at(middle);
        if (value == Sign::zero) {
            return {root.squarefree(), middle, middle, root.multiplicity()};
        }
        if (value == root.signAboveLower()) {
            lower = std::move(middle);
        } else {
            upper = std::move(middle);
        }
        span = span.timesPowerOfTwo(-1);
    }
    // The bracket of the root is the bracket of the same root here.
    RealRoot result{root.squarefree(), std::move(lower), std::move(upper), root.multiplicity()};
    result.m_below = root.m_below;
    result.m_above = root.m_above;
    return result;
}

double toDouble(const RealRoot& root) {
    if (root.isExact()) {
        return root.lower().toDouble();
    }
    const Dyadic absoluteWidth = powerOfTwo(kAbsoluteWidthExponent);
    BisectionSigns signs(root, root.m_below, root.m_above);
    Dyadic lower = root.lower();
    Dyadic upper = root.upper();
    while (true) {
        const Dyadic width = upper - lower;
        if (compare(width, absoluteWidth) != Sign::positive) {
            break;
        }
        const Sign lowerSign = lower.sign();
        if (lowerSign != Sign::zero && lowerSign == upper.sign()) {
            const Dyadic& nearer =
                lowerSign == Sign::positive ? lower : upper;  // the smaller in absolute value
            const Dyadic relativeWidth = nearer.abs().timesPowerOfTwo(-kRoundingBits);
            if (compare(width, relativeWidth) != Sign::positive) {
                break;
            }
        }
        Dyadic middle = midpoint(lower, upper);
        const Sign value = signs.at(middle);
        if (value == Sign::zero) {
            return middle.toDouble();
        }
        if (value == root.signAboveLower()) {
            lower = std::move(middle);
        } else {
            upper = std::move(middle);
        }
    }
    return midpoint(lower, upper).toDouble();
}

}  // namespace solid_space::geom2d::exact
