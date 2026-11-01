// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Numbers of a tower of square roots: exact arithmetic and exact signs for the
// nested radicals of offset lines and circles (ADR 0011, "Che cosa rende
// possibili decisioni esatte"; ADR 0012, point 4). Internal to geom2d. Stage
// 0.2, package P12; P14 and P15 use it without changing it.
//
// What a tower is. A Tower holds square roots sqrt(r_0), ..., sqrt(r_{k-1}),
// adjoined one after the other: each radicand r_i is a positive number of the
// tower made only of the roots before it, so roots may be nested
// (sqrt(5 + 2 * sqrt(2) * sqrt(3)), sqrt(L) * d + sqrt(rho)). A TowerNumber is a
// polynomial with dyadic coefficients in those roots, of degree at most one in
// each:
//     x = sum over the subsets S of {0, ..., k-1} of c_S * prod_{i in S} sqrt(r_i),
// with sqrt(r_i)^2 replaced by r_i whenever a product creates it. The value
// of x is the real number obtained with the positive square roots. Rational
// quotients are written in homogeneous coordinates by the callers (a numerator
// in the tower, a positive dyadic weight), as in carriers.hpp.
//
// The limit. A tower holds at most kMaxTowerRoots roots. The decisions of stage
// 0.2 need at most five together (the normals or radii of three consecutive
// pieces and the discriminants of the two joints of the middle one); one more
// is a margin. squareRoot() refuses a root beyond the limit, and says so: the
// caller turns that into a diagnosis, never into a silent fallback.
//
// Exactness. Sums, differences and products are exact. The sign is exact for
// every exponent, as in exact/dyadic.hpp, with the classical procedure: with
// x = a + b * sqrt(r), where a, b and r use only the roots below sqrt(r), the
// sign of x follows from the signs of a and b and, when they are opposite, from
// the sign of a^2 - b^2 * r, all one level lower. No threshold is involved.
// The representation is not unique (sqrt(2) * sqrt(8) and 4 are the same
// number), so a number is zero when sign() says so, never when its
// coefficients look so.
//
// Cost. A sign at depth k evaluates up to 3^k signs of products one level lower,
// and the coefficients grow with each squaring: bounded, because the depth is.
// A floating-point filter for the frequent decisions belongs to the callers
// (P14, P15), next to their own error bounds.
//
// Rounding. toDouble() and quotientToDouble() give a double within 2 ulp of the
// exact value (in fact within one), with no loss of accuracy when the terms of
// a number almost cancel: they rewrite a + b * sqrt(r) with opposite signs as
// (a^2 - b^2 * r) / (a - b * sqrt(r)), whose terms have the same sign.
//
// All the computation is in tower.cpp (ADR 0008, point 5). The functions may
// allocate, so they are not noexcept; std::bad_alloc is the only exception
// that may leave them (ADR 0003, point 5).

#include "dyadic.hpp"
#include "solid_space/robust/predicates.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace solid_space::geom2d::exact {

/// The largest number of roots in a Tower.
inline constexpr std::size_t kMaxTowerRoots = 6;

/// A number of a Tower: dyadic coefficients of the products of its roots. The
/// coefficient of index m multiplies the product of the roots sqrt(r_i) whose
/// bit i is set in m. A number of level n uses only the first n roots and has
/// 2^n coefficients; a number of a tower is also a number of every tower that
/// extends it (Tower::squareRoot only appends).
///
/// Sums, differences and products by a dyadic need no tower; products, signs
/// and rounding do (Tower). Numbers of different towers must not be mixed.
class TowerNumber {
public:
    /// Zero.
    TowerNumber() noexcept = default;

    /// A rational number of the tower.
    explicit TowerNumber(const Dyadic& value);

    /// The number of roots that the coefficients may use: the coefficients are
    /// 2^level() (or none for a number written as zero).
    [[nodiscard]] std::size_t level() const noexcept;

    /// The coefficients, by product of roots (see above). Empty for a number
    /// written as zero; otherwise 2^level() of them, and the upper half is not
    /// all zero.
    [[nodiscard]] std::span<const Dyadic> coefficients() const noexcept { return m_coefficients; }

    /// The coefficient of the product of the roots in `mask`, zero beyond the
    /// stored ones.
    [[nodiscard]] Dyadic coefficient(std::size_t mask) const;

    /// True when the number uses no root (its value is coefficient(0)).
    [[nodiscard]] bool isRational() const noexcept { return m_coefficients.size() <= 1; }

    /// The number made of a single product of roots: the roots whose bit is
    /// set in `mask`, times `factor`.
    [[nodiscard]] static TowerNumber monomial(std::size_t mask, const Dyadic& factor);

    /// The number times 2^power, exactly.
    [[nodiscard]] TowerNumber timesPowerOfTwo(std::int64_t power) const;

    friend TowerNumber operator+(const TowerNumber& a, const TowerNumber& b);
    friend TowerNumber operator-(const TowerNumber& a, const TowerNumber& b);
    friend TowerNumber operator-(const TowerNumber& a);
    friend TowerNumber operator*(const TowerNumber& a, const Dyadic& factor);
    friend TowerNumber operator*(const Dyadic& factor, const TowerNumber& a);

    /// True when the two numbers have the same coefficients: the same
    /// representation. Two equal numbers may differ in representation; compare
    /// values with Tower::compare().
    friend bool sameRepresentation(const TowerNumber& a, const TowerNumber& b);

private:
    friend class Tower;

    explicit TowerNumber(std::vector<Dyadic> coefficients);

    /// Restores the invariant of coefficients(): drops the upper halves that
    /// are all zero, and the last coefficient when it is zero.
    void trim();

    std::vector<Dyadic> m_coefficients;
};

// The friends again at namespace scope, where [[nodiscard]] applies.
[[nodiscard]] TowerNumber operator+(const TowerNumber& a, const TowerNumber& b);
[[nodiscard]] TowerNumber operator-(const TowerNumber& a, const TowerNumber& b);
[[nodiscard]] TowerNumber operator-(const TowerNumber& a);
[[nodiscard]] TowerNumber operator*(const TowerNumber& a, const Dyadic& factor);
[[nodiscard]] TowerNumber operator*(const Dyadic& factor, const TowerNumber& a);
[[nodiscard]] bool sameRepresentation(const TowerNumber& a, const TowerNumber& b);

/// The roots of a tower, and the operations that need them.
class Tower {
public:
    /// A tower with no roots: its numbers are the dyadic numbers.
    Tower() = default;

    /// The number of roots adjoined so far.
    [[nodiscard]] std::size_t size() const noexcept { return m_radicands.size(); }

    /// The radicands r_0, ..., r_{size()-1}, each positive and of a level
    /// lower than its own index.
    [[nodiscard]] std::span<const TowerNumber> radicands() const noexcept { return m_radicands; }

    /// The positive square root of `radicand`, as a number of this tower:
    /// - zero when the radicand is zero;
    /// - a dyadic number when the radicand is the square of one (a rational
    ///   radicand only: perfect squares of numbers with roots are not looked
    ///   for, and get a root of their own, which is still exact);
    /// - the root already adjoined for a radicand with the same representation;
    /// - otherwise a new root, appended to the tower.
    /// Nothing when a new root is needed and the tower already has
    /// kMaxTowerRoots, or (asserted in debug) when the radicand is negative.
    /// Precondition: `radicand` is a number of this tower.
    [[nodiscard]] std::optional<TowerNumber> squareRoot(const TowerNumber& radicand);

    /// The product a * b, exact.
    [[nodiscard]] TowerNumber product(const TowerNumber& a, const TowerNumber& b) const;

    /// The sign of the value of x, exact.
    [[nodiscard]] robust::Sign sign(const TowerNumber& x) const;

    /// The sign of a - b, exact.
    [[nodiscard]] robust::Sign compare(const TowerNumber& a, const TowerNumber& b) const;

    /// A double within 2 ulp of the value of x (in fact within one): a zero
    /// gives +0, a value beyond the largest double gives +-infinity. Accurate
    /// also when the terms of x almost cancel.
    [[nodiscard]] double toDouble(const TowerNumber& x) const;

    /// A double within 2 ulp of x / divisor, as toDouble(). Precondition
    /// (asserted in debug): divisor is positive.
    [[nodiscard]] double quotientToDouble(const TowerNumber& x, const Dyadic& divisor) const;

private:
    std::vector<TowerNumber> m_radicands;
};

}  // namespace solid_space::geom2d::exact
