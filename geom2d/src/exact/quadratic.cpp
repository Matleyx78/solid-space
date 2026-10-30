// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "quadratic.hpp"

#include <cassert>
#include <cstdint>

namespace solid_space::geom2d::exact {

namespace {

using robust::Sign;

// Bits of the square root used by quadraticToDouble: the approximate value has
// a relative error below 2^-127, far below the 2^-53 of half an ulp, so that
// the rounding of the quotient decides the result.
constexpr std::int64_t kRootBits = 128;

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

// The sign of p + q when the signs of p and q are known: it is decided at once
// unless they are opposite, and then it is the sign of p times the sign of
// p^2 - q^2, which `squaresDifference` computes only when it is needed.
template <typename SquaresDifference>
Sign signOfSum(Sign p, Sign q, SquaresDifference squaresDifference) {
    if (q == Sign::zero || p == q) {
        return p;
    }
    if (p == Sign::zero) {
        return q;
    }
    const Sign difference = squaresDifference();
    return p == Sign::positive ? difference : negated(difference);
}

}  // namespace

Sign signOf(const Dyadic& a, const Dyadic& b, const Dyadic& d) {
    assert(d.sign() != Sign::negative && "signOf needs a radicand that is not negative");
    const Sign radical = d.isZero() ? Sign::zero : b.sign();
    // a + b sqrt(d) with a and b sqrt(d) of opposite signs: compare a^2 and b^2 d.
    return signOfSum(a.sign(), radical, [&] { return compare(a * a, b * b * d); });
}

Sign signOf(const QuadraticNumber& x) {
    return signOf(x.a, x.b, x.d);
}

Sign signOf(const QuadraticNumber& x, const QuadraticNumber& y, const Dyadic& e) {
    assert(e.sign() != Sign::negative && "signOf needs a radicand that is not negative");
    assert((x.b.isZero() || y.b.isZero() || compare(x.d, y.d) == Sign::zero) &&
           "x and y must share their radical");
    const Sign radical = e.isZero() ? Sign::zero : signOf(y);
    return signOfSum(signOf(x), radical, [&] {
        // x^2 - y^2 e = A + B sqrt(d), with
        // A = x.a^2 + x.b^2 x.d - e (y.a^2 + y.b^2 y.d) and
        // B sqrt(d) = 2 x.a x.b sqrt(x.d) - 2 e y.a y.b sqrt(y.d), where the two
        // radicals are the same whenever both terms can be nonzero.
        const Dyadic rational = x.a * x.a + x.b * x.b * x.d - e * (y.a * y.a + y.b * y.b * y.d);
        const Dyadic coefficient = (x.a * x.b - e * y.a * y.b).timesPowerOfTwo(1);
        const Dyadic& radicand = x.b.isZero() ? y.d : x.d;
        return signOf(rational, coefficient, radicand);
    });
}

double quadraticToDouble(const Dyadic& a, const Dyadic& b, const Dyadic& d, const Dyadic& c) {
    assert(d.sign() != Sign::negative && "quadraticToDouble needs a radicand that is not negative");
    assert(!c.isZero() && "quadraticToDouble needs a denominator that is not zero");
    if (b.isZero() || d.isZero()) {
        return quotientToDouble(a, c);
    }
    // S <= sqrt(d) < S (1 + 2^(1 - kRootBits)).
    const Dyadic root = floorSqrt(d, kRootBits);
    if (a.isZero() || a.sign() == b.sign()) {
        // |a + b S| = |a| + |b| S: no cancellation, so its relative error is the
        // one of S.
        return quotientToDouble(a + b * root, c);
    }
    // a and b sqrt(d) have opposite signs and may cancel: use the conjugate,
    // a + b sqrt(d) = (a^2 - b^2 d) / (a - b sqrt(d)), whose numerator is exact
    // and whose denominator adds two numbers of the same sign.
    const Dyadic numerator = a * a - b * b * d;
    if (numerator.isZero()) {
        return quotientToDouble(numerator, c);
    }
    return quotientToDouble(numerator, c * (a - b * root));
}

}  // namespace solid_space::geom2d::exact
