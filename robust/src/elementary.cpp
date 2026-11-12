// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Error analysis (ADR 0016). Let u = 2^-53, with round-to-nearest binary64.
// All double-double operations are those analyzed in trigonometry.cpp.
//
// tan: for |angle| < 2^-27, tan(angle) differs from angle by less than
// 2^-55 |angle| (the cubic term is angle^3/3). Otherwise, before rounding,
// the reduced sine and cosine each have relative error below 2^-62
// (sinCos, steps 1 and 2). Taking their ratio, or its negative reciprocal
// in an odd quadrant, keeps the relative error below 2^-60 including
// the double-double division. The final rounding adds at most one ulp of
// the exact value even across a binade boundary (sinCos, step 3). The
// distance to the poles from sinCos, step 1, bounds the magnitude below
// 2^61. Using only the magnitude and negating at the end gives exact
// oddness, also when the positive argument has a negative tangent.
//
// acos: for a = |x| > 1/2, d = 1 - a is exact by Sterbenz's lemma and the
// square of the ordinate is 2d - d^2. For a <= 1/2 it is 1 - a^2.
// Neither subtraction loses more than a factor of two; the products are
// exact expansions and the resulting double-double has relative error
// O(u^2). The corrected root described below preserves that accuracy,
// including at a = 1 - 2^-53. For a < 2^-56, acos(x) rounds to pi/2:
// the perturbation is less than 2^-55, below the distance from pi/2
// to either rounding midpoint. Otherwise atan2's reduction (steps 1-4)
// gives the angle of (a, root.hi). Its relative error is below 2^-67,
// except for the small-ratio shortcut. That shortcut can occur only
// when a is the numerator (the smallest nonzero ordinate is >= 2^-26).
// The omitted cubic term is then below 2^-80 in absolute angle near
// pi/2; the rounding of the small ratio contributes less than 2^-79.
// The derivative of the angle in the ordinate is a/(a^2 + h^2) = a at
// the exact root h, so adding a * root.lo leaves an O(u^2) relative
// error (a * h is at most the angle). Reflecting in pi/2 for x < 0 does
// not amplify relative error. Final rounding as for tan is within 2 ulp.
// The endpoints are explicit and the rounded result is bounded by pi.
//
// hypot: order the absolute components first. If their frexp exponents
// differ by at least 28, the ratio b/a is below 2^-27 and the increment
// over a is below 2^-55 a, less than half the spacing above a. Otherwise
// scale the larger to [1/2, 1) and the smaller to [2^-28, 1). Both products
// are exact expansions and their double-double sum has relative error
// O(u^2), with no intermediate underflow or overflow. For the corrected
// root, r = RN(sqrt(sum.hi)) and fma(-r, r, sum.hi) is exact: the residual
// is a multiple of the square of the spacing of r and fits in 53 bits.
// Adding sum.lo and dividing by 2r gives the Newton correction. The
// omitted second-order term and rounding of the correction are O(u^2),
// so rounding r plus the correction to double gives an error of at most
// one ulp plus O(u^2) relative, or exactly the scaled root if representable.
// Scaling back is exact for normal results. In the subnormal range the
// final rounding is at most half a subnormal ulp; the rounding of the
// normalized root contributes at most half a subnormal ulp plus O(u^2)
// relative there, since its scaled spacing is at most 2^-1074. An exact
// representable length stays exact, including after subnormal scaling.
// The larger component is a lower bound and is positive for nonzero
// vectors. Sorting magnitudes makes all sign and order variants identical.

#include "solid_space/robust/elementary.hpp"

#include "trigonometry_internal.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

namespace solid_space::robust {
namespace {

using trig::DoubleDouble;

DoubleDouble correctedRoot(DoubleDouble square) noexcept {
    const double root = std::sqrt(square.hi);
    const double residual = std::fma(-root, root, square.hi) + square.lo;
    return trig::fastTwoSum(root, residual / (2.0 * root));
}

}  // namespace

double tan(double angle) noexcept {
    assert(std::isfinite(angle) && std::fabs(angle) <= kMaxTrigArgument);
    const double magnitude = std::fabs(angle);
    if (magnitude < trig::kSmallAngle) {
        return angle;
    }
    const auto [reduced, quadrant] = trig::reduce(magnitude);
    const DoubleDouble squared = trig::multiply(reduced, reduced);
    const DoubleDouble sine =
        trig::multiply(reduced, trig::series(trig::kSineHead, trig::kSineTail, squared));
    const DoubleDouble cosine = trig::series(trig::kCosineHead, trig::kCosineTail, squared);
    const double tangent =
        (quadrant & 1U) == 0U ? trig::divide(sine, cosine).hi : -trig::divide(cosine, sine).hi;
    return std::signbit(angle) ? -tangent : tangent;
}

double acos(double x) noexcept {
    assert(std::isfinite(x) && std::fabs(x) <= 1.0);
    const double magnitude = std::fabs(x);
    if (magnitude == 1.0) {
        return x > 0.0 ? 0.0 : trig::kPi.hi;
    }
    constexpr double kRoundsToPiOverTwo = 0x1p-56;
    if (magnitude < kRoundsToPiOverTwo) {
        return trig::kPiOverTwo.hi;
    }

    const bool nearEndpoint = magnitude > 0.5;
    const double factor = nearEndpoint ? 1.0 - magnitude : magnitude;
    const double head = nearEndpoint ? 2.0 * factor : 1.0;
    const DoubleDouble tail = trig::negate(trig::twoProduct(factor, factor));
    const DoubleDouble square = trig::add({head, 0.0}, tail);
    const DoubleDouble height = correctedRoot(square);
    const double numerator = std::min(height.hi, magnitude);
    const double denominator = std::max(height.hi, magnitude);
    DoubleDouble angle = trig::arctangent(numerator, denominator);
    if (height.hi > magnitude) {
        angle = trig::add(trig::kPiOverTwo, trig::negate(angle));
    }
    angle = trig::add(angle, trig::twoProduct(magnitude, height.lo));
    if (x < 0.0) {
        angle = trig::add(trig::kPi, trig::negate(angle));
    }
    return std::min(angle.hi, trig::kPi.hi);
}

double hypot(double x, double y) noexcept {
    assert(std::isfinite(x) && std::fabs(x) <= kMaxHypotArgument);
    assert(std::isfinite(y) && std::fabs(y) <= kMaxHypotArgument);
    double larger = std::fabs(x);
    double smaller = std::fabs(y);
    if (larger < smaller) {
        std::swap(larger, smaller);
    }
    if (smaller == 0.0) {
        return larger;
    }

    int largerExponent = 0;
    int smallerExponent = 0;
    const double scaledLarger = std::frexp(larger, &largerExponent);
    const double smallerFraction = std::frexp(smaller, &smallerExponent);
    const int exponentGap = largerExponent - smallerExponent;
    constexpr int kNegligibleExponentGap = 28;
    if (exponentGap >= kNegligibleExponentGap) {
        return larger;
    }
    const double scaledSmaller = std::ldexp(smallerFraction, -exponentGap);
    const DoubleDouble largerSquared = trig::twoProduct(scaledLarger, scaledLarger);
    const DoubleDouble smallerSquared = trig::twoProduct(scaledSmaller, scaledSmaller);
    const DoubleDouble square = trig::add(largerSquared, smallerSquared);
    return std::max(larger, std::ldexp(correctedRoot(square).hi, largerExponent));
}

}  // namespace solid_space::robust
