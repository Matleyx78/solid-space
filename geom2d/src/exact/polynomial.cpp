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

#include "polynomial.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
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
    const Polynomial squarefree = squarefreePart(p);
    const std::vector<Polynomial> terms = sturmTerms(squarefree);

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
    stack.push_back({-bound, signChangesAt(terms, -bound), bound, signChangesAt(terms, bound)});
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
        if (squarefree.signAt(middle) != Sign::zero) {
            const std::size_t changesMiddle = signChangesAt(terms, middle);
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
            if (squarefree.signAt(below) != Sign::zero && squarefree.signAt(above) != Sign::zero) {
                changesBelow = signChangesAt(terms, below);
                changesAbove = signChangesAt(terms, above);
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
    Dyadic lower = root.lower();
    Dyadic upper = root.upper();
    while (compare(upper - lower, width) == Sign::positive) {
        Dyadic middle = midpoint(lower, upper);
        const Sign value = root.squarefree().signAt(middle);
        if (value == Sign::zero) {
            return {root.squarefree(), middle, middle, root.multiplicity()};
        }
        if (value == root.signAboveLower()) {
            lower = std::move(middle);
        } else {
            upper = std::move(middle);
        }
    }
    return {root.squarefree(), std::move(lower), std::move(upper), root.multiplicity()};
}

double toDouble(const RealRoot& root) {
    if (root.isExact()) {
        return root.lower().toDouble();
    }
    const Dyadic absoluteWidth = powerOfTwo(kAbsoluteWidthExponent);
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
        const Sign value = root.squarefree().signAt(middle);
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
