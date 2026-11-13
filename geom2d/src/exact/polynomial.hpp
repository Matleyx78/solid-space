// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Real roots of polynomials of degree at most four, with exact decisions (ADR
// 0017, "Che cosa rende possibili decisioni esatte"). Internal to geom2d.
// Stage 0.3, package PS3; PS7 (intersections of the ellipse) and PS8 (nearest
// point of the ellipse) use it without changing it.
//
// Why it exists. An ellipse meets a circle or another ellipse at the real
// roots of a polynomial of degree at most four in the parameter of its
// rational form, and the feet of the perpendiculars from a point are the roots
// of a quartic too. Such roots are not in general in a tower of square roots
// (exact/tower.hpp), so the decisions on them (how many, with which
// multiplicity, where they lie with respect to the ends of an arc, in which
// order) need the tools below.
//
// What it offers.
// - Polynomial: dyadic coefficients (exact/dyadic.hpp), with sum, difference,
//   product, derivative and exact evaluation. The degree and the leading
//   coefficient are exact: a zero leading coefficient lowers the degree.
// - greatestCommonDivisor() and squarefreePart(), from pseudo-remainders: no
//   fractions at all, so nothing to round.
// - SturmSequence: the exact number of distinct real roots on the whole line
//   or in a closed interval with dyadic ends.
// - realRoots(): the distinct real roots in increasing order, each isolated in
//   an interval with dyadic ends, with its multiplicity (RealRoot).
// - signAt(): the exact sign of another polynomial at a root, zero included
//   (a common root).
// - compare(): two roots of different polynomials, or a root and a dyadic
//   number: equal, before or after, exactly.
// - narrowed() and toDouble(): an isolating interval shrunk to a given width,
//   to build points within the errors the callers declare, and a root rounded
//   to a double with a written error.
//
// The degree limit. Every function that works on roots takes polynomials of
// degree at most kMaxDegree, the other polynomial of signAt() and the
// polynomials of compare() included (asserted in debug). The arithmetic of
// Polynomial has no limit: a product of two quartics is a valid polynomial,
// only not one whose roots these functions look for. PS7 needs at most degree
// two for the positions on the arcs (a cross product with the direction of an
// end, linear in the coordinates of the rational form), and four for the
// order of the roots.
//
// Exactness. Every decision is exact for every exponent of the coefficients,
// like the numbers of exact/dyadic.hpp: no threshold, no floating point. The
// algorithms are the classical ones of real algebraic geometry (S. Basu,
// R. Pollack, M.-F. Roy, Algorithms in Real Algebraic Geometry, chapter 2):
// - pseudo-remainders: the remainder of |lc(b)|^(deg a - deg b + 1) * a by b,
//   whose coefficients are polynomial in those of a and b. The positive factor
//   keeps the signs that Sturm sequences need;
// - Sturm's theorem: with s squarefree and V(x) the number of sign changes of
//   its Sturm sequence at x (zeros skipped), the distinct roots in (a, b] are
//   V(a) - V(b), for any a < b;
// - the Tarski query (Sylvester's theorem): with (a, b) an interval whose ends
//   are not roots of s, the sign changes of the signed remainder sequence of s
//   and s' * q give the sum of the signs of q at the roots of s in (a, b): for
//   an isolating interval, the exact sign of q at the root, zero included.
//   No refinement is needed, however close another root of q is;
// - a root bound in the style of Fujiwara, as a power of two, so that
//   isolation starts from an interval close to the size of the largest root
//   (coefficients near 2^-1074 or near 2^200 do not cost a thousand
//   bisections to reach their roots).
//
// Cost. It stays bounded because the degree is: a remainder sequence of a
// quartic has at most five terms, and the size of their coefficients grows by
// a bounded factor at each step (the remainders are not reduced by their
// content: Dyadic has no exact division). Isolation bisects until each
// interval holds one root, so its cost grows with the logarithm of the
// smallest distance between two roots; roots 2^-60 apart cost sixty
// bisections. narrowed() and toDouble() bisect too, one bit per step. A
// floating-point filter for the frequent cases far from multiple roots
// belongs to the callers, next to their own error bounds (ADR 0017,
// Decisione, point 9): the answers here are the exact ones.
//
// All the computation is in polynomial.cpp (ADR 0008, point 5). The functions
// may allocate, so they are not noexcept; std::bad_alloc is the only exception
// that may leave them (ADR 0003, point 5).

#include "dyadic.hpp"
#include "solid_space/robust/predicates.hpp"

#include <cstddef>
#include <initializer_list>
#include <span>
#include <vector>

namespace solid_space::geom2d::exact {

/// The largest degree of the polynomials whose roots the functions below look
/// for (see "The degree limit" above).
inline constexpr int kMaxDegree = 4;

/// A polynomial c_0 + c_1 x + ... + c_n x^n with dyadic coefficients.
class Polynomial {
public:
    /// The zero polynomial.
    Polynomial() noexcept = default;

    /// The polynomial with these coefficients, lowest degree first. Zero
    /// coefficients at the end are dropped, so the degree is exact.
    explicit Polynomial(std::vector<Dyadic> coefficients);

    /// The same, from a list: Polynomial({Dyadic(-2.0), Dyadic(), Dyadic(1.0)})
    /// is x^2 - 2.
    Polynomial(std::initializer_list<Dyadic> coefficients);

    /// The degree; -1 for the zero polynomial.
    [[nodiscard]] int degree() const noexcept;

    [[nodiscard]] bool isZero() const noexcept { return m_coefficients.empty(); }

    /// The coefficients, lowest degree first: degree() + 1 of them, the last
    /// one not zero (none for the zero polynomial).
    [[nodiscard]] std::span<const Dyadic> coefficients() const noexcept { return m_coefficients; }

    /// The coefficient of x^power, zero beyond the degree.
    [[nodiscard]] Dyadic coefficient(std::size_t power) const;

    /// The coefficient of x^degree(); zero for the zero polynomial.
    [[nodiscard]] Dyadic leadingCoefficient() const;

    /// The derivative.
    [[nodiscard]] Polynomial derivative() const;

    /// The exact value at x.
    [[nodiscard]] Dyadic evaluate(const Dyadic& x) const;

    /// The sign of the value at x, exact.
    [[nodiscard]] robust::Sign signAt(const Dyadic& x) const;

    friend Polynomial operator+(const Polynomial& a, const Polynomial& b);
    friend Polynomial operator-(const Polynomial& a, const Polynomial& b);
    friend Polynomial operator-(const Polynomial& a);
    friend Polynomial operator*(const Polynomial& a, const Polynomial& b);
    friend Polynomial operator*(const Polynomial& a, const Dyadic& factor);
    friend Polynomial operator*(const Dyadic& factor, const Polynomial& a);

    /// True when the two polynomials have the same coefficients.
    friend bool operator==(const Polynomial& a, const Polynomial& b);

private:
    /// Drops the zero coefficients at the end.
    void trim();

    std::vector<Dyadic> m_coefficients;
};

// The friends again at namespace scope, where [[nodiscard]] applies.
[[nodiscard]] Polynomial operator+(const Polynomial& a, const Polynomial& b);
[[nodiscard]] Polynomial operator-(const Polynomial& a, const Polynomial& b);
[[nodiscard]] Polynomial operator-(const Polynomial& a);
[[nodiscard]] Polynomial operator*(const Polynomial& a, const Polynomial& b);
[[nodiscard]] Polynomial operator*(const Polynomial& a, const Dyadic& factor);
[[nodiscard]] Polynomial operator*(const Dyadic& factor, const Polynomial& a);
[[nodiscard]] bool operator==(const Polynomial& a, const Polynomial& b);

/// A greatest common divisor of a and b, up to a nonzero dyadic factor: its
/// degree and its roots (with their multiplicities) are exact, its leading
/// coefficient is not normalized. Zero only when a and b are both zero; a
/// nonzero constant when they have no common complex root. Preconditions
/// (asserted in debug): the degrees are at most kMaxDegree.
[[nodiscard]] Polynomial greatestCommonDivisor(const Polynomial& a, const Polynomial& b);

/// A polynomial with the same complex roots as p, each with multiplicity one:
/// p divided by greatestCommonDivisor(p, p'), up to a nonzero dyadic factor;
/// p itself when it has no repeated factor. A nonzero constant for a constant
/// p. Preconditions (asserted in debug): p is not zero, its degree is at most
/// kMaxDegree.
[[nodiscard]] Polynomial squarefreePart(const Polynomial& p);

/// The Sturm sequence of the squarefree part of a polynomial: counts its
/// distinct real roots exactly.
class SturmSequence {
public:
    /// The sequence of squarefreePart(p). Preconditions (asserted in debug):
    /// p is not zero, its degree is at most kMaxDegree.
    explicit SturmSequence(const Polynomial& p);

    /// The number of distinct real roots of p.
    [[nodiscard]] std::size_t countRoots() const;

    /// The number of distinct real roots of p in the closed interval
    /// [lower, upper]. Precondition (asserted in debug): lower <= upper.
    [[nodiscard]] std::size_t countRoots(const Dyadic& lower, const Dyadic& upper) const;

    /// The squarefree part that the sequence starts with.
    [[nodiscard]] const Polynomial& squarefree() const noexcept { return m_terms.front(); }

private:
    // squarefreePart(p), its derivative and the negated pseudo-remainders, to
    // the last nonzero one (a nonzero constant).
    std::vector<Polynomial> m_terms;
};

/// A real root of a polynomial, isolated: an interval with dyadic ends that
/// contains it and no other root of the polynomial.
///
/// Either lower() == upper(), and the root is that dyadic number (isExact()),
/// or lower() < root < upper(), and the open interval (lower(), upper()) holds
/// no other root of the polynomial; its ends are not roots. Intervals of
/// different roots of the same polynomial, as realRoots() gives them, do not
/// overlap: they may share an end, which is then a root of neither.
///
/// A RealRoot keeps the squarefree part of its polynomial, so the functions
/// below need nothing else. It is a value: copying it copies a polynomial of
/// degree at most kMaxDegree.
class RealRoot {
public:
    /// The lower end of the isolating interval.
    [[nodiscard]] const Dyadic& lower() const noexcept { return m_lower; }

    /// The upper end of the isolating interval.
    [[nodiscard]] const Dyadic& upper() const noexcept { return m_upper; }

    /// True when the root is the dyadic number lower() == upper().
    [[nodiscard]] bool isExact() const noexcept { return m_exact; }

    /// The multiplicity of the root in the polynomial given to realRoots(): 1
    /// for a simple root, 2 for a double one (a tangency), and so on.
    [[nodiscard]] int multiplicity() const noexcept { return m_multiplicity; }

    /// The squarefree part of the polynomial (squarefreePart()): the root is a
    /// simple root of it.
    [[nodiscard]] const Polynomial& squarefree() const noexcept { return m_squarefree; }

    /// The sign of squarefree() at lower(): the sign it has on the whole
    /// interval from lower() to the root, the opposite one from the root to
    /// upper(). Zero for an exact root.
    [[nodiscard]] robust::Sign signAboveLower() const noexcept { return m_signAboveLower; }

private:
    friend std::vector<RealRoot> realRoots(const Polynomial& p);
    friend RealRoot narrowed(const RealRoot& root, const Dyadic& width);

    RealRoot(Polynomial squarefree, Dyadic lower, Dyadic upper, int multiplicity);

    Polynomial m_squarefree;
    Dyadic m_lower;
    Dyadic m_upper;
    int m_multiplicity = 1;
    bool m_exact = false;
    robust::Sign m_signAboveLower = robust::Sign::zero;
};

/// The distinct real roots of p, in increasing order, each with its
/// multiplicity (the multiplicities add up to at most the degree; to exactly
/// the degree when every complex root is real). Empty for a constant.
/// Preconditions (asserted in debug): p is not zero, its degree is at most
/// kMaxDegree.
[[nodiscard]] std::vector<RealRoot> realRoots(const Polynomial& p);

/// The sign of q at the root, exact: zero when the root is a root of q.
/// Precondition (asserted in debug): the degree of q is at most kMaxDegree.
[[nodiscard]] robust::Sign signAt(const Polynomial& q, const RealRoot& root);

/// The sign of root - x, exact.
[[nodiscard]] robust::Sign compare(const RealRoot& root, const Dyadic& x);

/// The sign of a - b, exact: zero when they are the same real number, also as
/// roots of different polynomials (a root of p and the same root of p * q).
[[nodiscard]] robust::Sign compare(const RealRoot& a, const RealRoot& b);

/// The same root, with an isolating interval at most `width` wide, a
/// sub-interval of the one given; an exact root is returned as it is. The
/// interval halves at each step, so the cost grows with
/// log2((upper - lower) / width). Precondition (asserted in debug): width > 0.
[[nodiscard]] RealRoot narrowed(const RealRoot& root, const Dyadic& width);

/// The root rounded to a double: |toDouble(r) - r| <= 2^-52 * |r| + 2^-1074
/// (within one ulp: half an ulp from rounding, far less than an ulp from the
/// isolating interval), exact when the root is a double. A root beyond the
/// largest double gives +-infinity, a root below half the smallest subnormal
/// a zero with its sign.
[[nodiscard]] double toDouble(const RealRoot& root);

}  // namespace solid_space::geom2d::exact
