// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Exact signs of numbers with one or two square roots, and the doubles that
// constructions return for them (ADR 0010 and ADR 0011, "Che cosa rende
// possibili decisioni esatte"). Internal to geom2d.
//
// Every coordinate that the decisions of geom2d compare has the form
// (a + b * sqrt(d)) / c, or combines two such radicals, with a, b, c, d exact
// polynomials in the given doubles. In homogeneous coordinates the division is
// not needed: the decisions take the sign of a numerator and of c apart.
// Nested radicals (offset lines and circles) are not here: P12 builds them on
// top of these numbers.
//
// The computation is in quadratic.cpp (ADR 0008, point 5). The functions may
// allocate (see dyadic.hpp), so they are not noexcept.

#include "dyadic.hpp"
#include "solid_space/robust/predicates.hpp"

namespace solid_space::geom2d::exact {

/// The number a + b * sqrt(d). Invariant expected by the functions below:
/// d >= 0.
struct QuadraticNumber {
    Dyadic a;
    Dyadic b;
    Dyadic d;
};

/// The sign of a + b * sqrt(d). Precondition (asserted in debug): d >= 0.
[[nodiscard]] robust::Sign signOf(const Dyadic& a, const Dyadic& b, const Dyadic& d);

/// The sign of x.a + x.b * sqrt(x.d).
[[nodiscard]] robust::Sign signOf(const QuadraticNumber& x);

/// The sign of x + y * sqrt(e), where x and y are numbers of the form
/// p + q * sqrt(d): two different radicals, and their product, as when points
/// of different pairs are compared along a curve (ADR 0010, intersectAlong).
/// Example: sqrt(2) + sqrt(8) - 3 * sqrt(2) is x = -2 * sqrt(2), y = 1, e = 8.
///
/// Preconditions (asserted in debug): x.d >= 0, y.d >= 0, e >= 0, and when
/// both x.b and y.b are not zero, x.d and y.d are the same number (one radical
/// sqrt(d), the other sqrt(e)).
[[nodiscard]] robust::Sign signOf(const QuadraticNumber& x, const QuadraticNumber& y,
                                  const Dyadic& e);

/// A double within 2 ulp of (a + b * sqrt(d)) / c, for the constructions: in
/// fact within one ulp, and exact when the quotient is a double (a perfect
/// square d, or b zero, included). A quotient that is exactly zero gives a
/// zero; one beyond the largest double gives +-infinity. The result does not
/// lose accuracy when a and b * sqrt(d) almost cancel.
///
/// Preconditions (asserted in debug): d >= 0, c != 0.
[[nodiscard]] double quadraticToDouble(const Dyadic& a, const Dyadic& b, const Dyadic& d,
                                       const Dyadic& c);

}  // namespace solid_space::geom2d::exact
