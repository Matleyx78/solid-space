// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "solid_space/robust/predicates.hpp"

#include "exact_sum.hpp"

#include <cassert>
#include <cmath>

namespace solid_space::robust {
namespace {

[[maybe_unused]] bool inDomain(Point2 point) noexcept {
    // False for NaN as well as for infinities and values beyond the domain.
    return std::fabs(point.x) <= kMaxCoordinate && std::fabs(point.y) <= kMaxCoordinate;
}

// The floating-point filter.
//
// The determinant is D = A * B - C * Q with A = bx - ax, B = cy - ay,
// C = by - ay, Q = cx - ax. In doubles (u = 2^-53, eta = 2^-1075):
// - a difference of two doubles has relative error at most u and no absolute
//   error (a result in the subnormal range is exact);
// - a product has error at most u * |exact| + eta, eta for the subnormals;
// - so each computed product P^ of the computed differences is within
//   g * |P| + eta of the exact product P, with g = 3u + 3u^2 + u^3, and
//   |P| <= (|P^| + eta) / (1 - g);
// - the final difference adds u * (|P^| + |Q^|).
// With S = |P^| + |Q^|, the error of the computed D^ is at most
//   S * (g / (1 - g) + u) + 2 * eta / (1 - g) <= 4u * (1 + 5u) * S + 3 * eta.
// The bound is itself computed in doubles: S^ = fl(|P^| + |Q^|) >= S * (1 - u),
// and two more roundings, the product (which may lose eta) and the sum. With
// the coefficient 4u * (1 + 2^-30) and the absolute term 2^-1070 = 32 * eta,
//   bound^ >= 4u * (1 + 2^-30) * (1 - u)^3 * S + 30 * eta
//          >= 4u * (1 + 5u) * S + 3 * eta,
// so |D^| > bound^ proves that D has the sign of D^. Nothing overflows: the
// coordinates are at most 2^50, the differences 2^51, the products 2^102.
constexpr double kFilterCoefficient = 0x1.00000004p-51;  // 4u * (1 + 2^-30)
constexpr double kFilterAbsolute = 0x1p-1070;            // 32 * eta

}  // namespace

Sign orient2d(Point2 a, Point2 b, Point2 c) noexcept {
    assert(inDomain(a) && inDomain(b) && inDomain(c));
    const double left = (b.x - a.x) * (c.y - a.y);
    const double right = (b.y - a.y) * (c.x - a.x);
    const double determinant = left - right;
    const double bound =
        kFilterCoefficient * (std::fabs(left) + std::fabs(right)) + kFilterAbsolute;
    if (determinant > bound) {
        return Sign::positive;
    }
    if (determinant < -bound) {
        return Sign::negative;
    }
    // The exact determinant, developed into products of the given doubles
    // (the term ax * ay cancels):
    //   ax * by - ax * cy + bx * cy - bx * ay + cx * ay - cx * by.
    exact::ExactSum sum;
    sum.add(a.x, b.y);
    sum.subtract(a.x, c.y);
    sum.add(b.x, c.y);
    sum.subtract(b.x, a.y);
    sum.add(c.x, a.y);
    sum.subtract(c.x, b.y);
    return sum.sign();
}

}  // namespace solid_space::robust
