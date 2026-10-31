// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// sinCos and atan2 with the same bits on every platform (ADR 0008, point 5;
// ADR 0010). Only operations that IEEE 754 rounds correctly: +, -, *, /, fma,
// round, fabs, copysign; the target is built without FMA contraction (P3).
// The constants and how they were computed are in trigonometry_constants.hpp.
//
// Notation: u = 2^-53, RN() rounds to nearest, ulp(v) is the spacing of the
// doubles in the binade of v. Most of the work is done in double-double
// arithmetic (an unevaluated sum hi + lo of two doubles): twoSum, fastTwoSum
// and twoProduct are exact, and each other double-double operation below has
// a relative error of at most 2^-100 (at most 5u^2 for a product, 3u^2 times
// the cancellation factor for a sum, 2^-100 for a quotient). No sum below
// cancels by more than a factor 3 and no chain has more than 25 operations,
// so all double-double roundings together stay below 2^-94.
//
// sinCos(x)
//
// 0. |x| < 2^-27: the result is (x, 1). |sin x - x| <= |x|^3 / 6 < 2^-55 |x|
//    and 1 - cos x <= x^2 / 2 < 2^-55: at most a quarter of an ulp each.
//
// 1. Reduction: q = round(RN(x * RN(2/pi))) and r = x - q * pi/2, so that
//    sin x and cos x are sin r and cos r with signs and order set by q mod 4.
//    For |x| <= 2^20 the computed product is within 2^-32 of x * 2/pi, so
//    |r| <= pi/4 + 2^-31 = rho < 0.7854 and |q| < 2^20.
//    pi/2 is split in five parts P1..P5 (trigonometry_constants.hpp). The
//    first four have at most 33 significant bits, so q * Pi is exact. r1 =
//    x - q * P1 is exact too: if q != 0 then |x| > 1/2, so ulp(x) is between
//    2^-53 and 2^-32; x and q * P1 (a multiple of 2^-32) are multiples of
//    ulp(x), and |r1| < rho + 2^-12 < 1 <= 2^53 ulp(x). The other parts are
//    subtracted with twoSum, which keeps the exact errors e_i; their sum is
//    rounded three times, at most 12u^2 (|r| + 2^-47) in all, since each
//    partial sum is within 2^-47 of r. q * P5 is rounded (error < 2^-164) and
//    pi/2 minus the five parts is below 2^-187 (q times it, below 2^-167).
//    Why five parts suffice: when q != 0, |r| >= 2^-60.49 for every double
//    |x| <= 2^20. The bound comes from an exhaustive search over q (with
//    exact rational pi): for each q the double nearest to q * pi/2 is the one
//    closest to it, and the closest of all is x = 0x1.6c6cbc45dc8dep+5
//    (q = 29). The relative error of the reduced r = rh + rl is therefore at
//    most 12u^2 + 2^-149.4 / 2^-60.49 < 2^-88; when q = 0, r = x exactly.
//
// 2. Polynomials, with z = r^2 (a double-double, relative error < 2^-87):
//    sin r = r * S(z) and cos r = C(z), Taylor series (constants file). The
//    high terms (n >= 3) are a Horner scheme in doubles on z.hi, with fma:
//    each step adds a term at least 56 times larger than the rest, so the
//    computed tail is within 4u of the exact one. The low terms (n <= 2) and
//    the product by r are in double-double.
//    - sin: |z| <= rho^2 < 0.617, the tail is below 1/7! < 2^-12.29 and is
//      multiplied by z^3 <= 2^-2.09: its error is at most 4u * 2^-14.38 =
//      2^-65.4; S(z) >= sin(rho) / rho > 0.9, so 2^-65.2 relative. The first
//      term left out, z^10 / 21!, is below 2^-72 relative.
//    - cos: the tail is below 1/6! < 2^-9.49, its error at most 4u * 2^-11.58
//      = 2^-62.6; C(z) >= cos(rho) > 0.707, so 2^-62.1 relative. The first
//      term left out, z^11 / 22!, is below 2^-77 relative.
//    The error of r moves sin r and cos r by at most its own relative error
//    (|r tan r| <= 0.8 for the cosine).
//    In all: relative error delta <= 2^-62.1 + 2^-72 + 2^-88 + 2^-94 < 2^-62.
//
// 3. Reconstruction: a swap and sign changes, exact. The double returned is
//    RN(y) of the double-double y, with |y - v| <= delta |v| for the exact v.
//    |RN(y) - v| <= ulp(y) / 2 + delta |v| <= ulp(v) + 2^53 delta ulp(v),
//    because ulp(y) <= 2 ulp(v) and |v| < 2^53 ulp(v): at most 1.002 ulp,
//    below kTrigMaxErrorUlps (2). The bound is 1/2 ulp + 2^-9 ulp when y and v
//    are in the same binade, which is almost always. Since |v| <= 1 and y is
//    within 2^-62 of v, RN(y) never leaves [-1, 1].
//
// Symmetry: every step is odd in x (round, products, twoSum and the
// polynomials of r are), q mod 4 is taken on the two's complement, and the
// cosine depends on r only through z, so sinCos(-x) is (-sine, cosine) bit
// for bit.
//
// atan2(y, x)
//
// 0. y = +-0: +-0 if x is +0 or positive, +-pi (RN(pi), the sign of y) if x
//    is -0 or negative, as in the C standard (issue #31). With y != 0 and
//    x = +-0 the general path below gives +-RN(pi/2), as the C standard does.
//
// 1. With a = |y| and b = |x|, the angle is phi = atan(t) for t = min / max
//    in [0, 1], then pi/2 - phi if a > b, then pi - (that) if x < 0, with the
//    sign of y. When max < 2^-511 both are multiplied by 2^600 (exact), so
//    nothing below underflows. t = th + tl with th = RN(min / max) and tl =
//    RN(rem / max), where rem = min - th * max (one fma) is exact: it is a
//    multiple of ulp(th) ulp(max) >= 2^-1074 below ulp(th) max / 2. So t is
//    known within u^2 t, which moves atan(t) by less than 2^-105.6 relative.
//    If th < 2^-27, phi = th: |atan t - th| <= ulp(t) / 2 + t^3 / 3, at most
//    1.2 ulp of atan t when phi is the result, and below 2^-80 when phi is
//    then subtracted from pi/2 or pi.
//
// 2. Reduction: c = j / 8 with j = round(8 th), so |t - c| <= 1/16 + u^2, and
//    atan t = atan c + atan(U), U = (t - c) / (1 + c t), |U| < 2^-4. t - c is
//    exact (th - c is exact by Sterbenz's lemma, since c >= 1/8 or c = 0) and
//    1 + c t is a double-double within 2^-103; U is a double-double within
//    2^-99.
//
// 3. Polynomial: atan U = U * A(w), w = U^2 <= 2^-8, Taylor series. Tail
//    (n >= 2) in doubles: below 1/5 * 1.01, error at most 4u * 0.202, times
//    w^2 <= 2^-16: 2^-68.3, and A(w) > 0.998. The first term left out,
//    w^9 / 19, is below 2^-76. atan(c) is a double-double within 2^-106 and
//    |atan U| <= atan(t) (if j = 0 they are equal; otherwise |atan U| <=
//    atan(1/16) <= atan(t)), so atan t is within 2^-68 + 2^-94 < 2^-67.9
//    relative.
//
// 4. Quadrant: pi/2 - phi >= pi/4 >= phi and pi - psi >= pi/2 >= psi, so the
//    subtractions keep the relative error, and add 2^-106 for the constants.
//    delta < 2^-67 and, as in step 3 of sinCos, the result is within 1.0001
//    ulp. The exact value is at most pi and the result within 2^-65 of it, far
//    from the midpoint between RN(pi) and the next double (pi + 2^-53.15):
//    the result is never beyond +-RN(pi).
//
// Symmetry: atan2(-y, x) = -atan2(y, x) bit for bit, because only |y| is
// used and the sign is copied at the end.

#include "solid_space/robust/trigonometry.hpp"

#include "trigonometry_constants.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <utility>

namespace solid_space::robust {
namespace {

using trig::DoubleDouble;

/// Below this magnitude sinCos returns (x, 1) (step 0 above).
constexpr double kSmallAngle = 0x1p-27;

/// Below this ratio atan(t) is t (atan2, step 1).
constexpr double kSmallRatio = 0x1p-27;

/// A larger of |x|, |y| below kSmallMagnitude is scaled by kScaleUp (exact).
constexpr double kSmallMagnitude = 0x1p-511;
constexpr double kScaleUp = 0x1p600;

/// The centres of the arctangent reduction are multiples of 1/kEighths.
constexpr double kEighths = 8.0;

/// a + b exactly, with no condition on a and b.
DoubleDouble twoSum(double a, double b) noexcept {
    const double sum = a + b;
    const double bVirtual = sum - a;
    const double aVirtual = sum - bVirtual;
    return {sum, (a - aVirtual) + (b - bVirtual)};
}

/// a + b exactly, when a = 0 or |a| >= |b|.
DoubleDouble fastTwoSum(double a, double b) noexcept {
    const double sum = a + b;
    return {sum, b - (sum - a)};
}

/// a * b exactly (no underflow here: see the header comment).
DoubleDouble twoProduct(double a, double b) noexcept {
    const double product = a * b;
    return {product, std::fma(a, b, -product)};
}

DoubleDouble negate(DoubleDouble a) noexcept {
    return {-a.hi, -a.lo};
}

DoubleDouble add(DoubleDouble a, DoubleDouble b) noexcept {
    const DoubleDouble high = twoSum(a.hi, b.hi);
    const DoubleDouble low = twoSum(a.lo, b.lo);
    const DoubleDouble partial = fastTwoSum(high.hi, high.lo + low.hi);
    return fastTwoSum(partial.hi, partial.lo + low.lo);
}

DoubleDouble multiply(DoubleDouble a, DoubleDouble b) noexcept {
    const DoubleDouble product = twoProduct(a.hi, b.hi);
    const double low = std::fma(a.lo, b.hi, std::fma(a.hi, b.lo, product.lo));
    return fastTwoSum(product.hi, low);
}

DoubleDouble divide(DoubleDouble a, DoubleDouble b) noexcept {
    const double quotient = a.hi / b.hi;
    // a - quotient * b: a.hi - product.hi is exact (Sterbenz's lemma).
    const DoubleDouble product = twoProduct(quotient, b.hi);
    const double remainder = (((a.hi - product.hi) - product.lo) + a.lo) - quotient * b.lo;
    return fastTwoSum(quotient, remainder / b.hi);
}

/// sum over n of coefficient_n * z^n: the tail in doubles on z.hi, then the
/// head in double-double (steps 2 and 3 of the header comment).
template <std::size_t HeadSize, std::size_t TailSize>
DoubleDouble series(const std::array<DoubleDouble, HeadSize>& head,
                    const std::array<double, TailSize>& tail, DoubleDouble z) noexcept {
    double tailValue = 0.0;
    for (auto coefficient = tail.rbegin(); coefficient != tail.rend(); ++coefficient) {
        tailValue = std::fma(tailValue, z.hi, *coefficient);
    }
    DoubleDouble value{tailValue, 0.0};
    for (auto coefficient = head.rbegin(); coefficient != head.rend(); ++coefficient) {
        value = add(*coefficient, multiply(z, value));
    }
    return value;
}

struct Reduction {
    DoubleDouble angle;
    std::uint64_t quadrant = 0;  // q mod 4
};

Reduction reduce(double x) noexcept {
    const double quotient = std::round(x * trig::kTwoOverPi);
    const auto& parts = trig::kPiOverTwoParts;
    double head = x - quotient * parts.front();  // exact
    double low = 0.0;
    for (const double part : std::span(parts).subspan(1)) {
        const DoubleDouble step = twoSum(head, -(quotient * part));
        head = step.hi;
        low += step.lo;
    }
    // |quotient| < 2^20: the conversion is exact, and the cast to unsigned
    // keeps q mod 4 for a negative q (two's complement).
    const auto quadrant = static_cast<std::uint64_t>(static_cast<std::int64_t>(quotient));
    return {fastTwoSum(head, low), quadrant & 3U};
}

/// atan(numerator / denominator) in [0, pi/4], for 0 <= numerator <=
/// denominator, denominator >= 2^-511 (atan2, steps 1 to 3).
DoubleDouble arctangent(double numerator, double denominator) noexcept {
    const double quotient = numerator / denominator;
    if (quotient < kSmallRatio) {
        return {quotient, 0.0};
    }
    const double remainder = std::fma(-quotient, denominator, numerator);  // exact
    const double quotientLow = remainder / denominator;
    const double eighths = std::round(quotient * kEighths);
    const double centre = eighths / kEighths;
    // t - c, exactly: quotient - centre is exact.
    const DoubleDouble difference = twoSum(quotient - centre, quotientLow);
    // 1 + c * t.
    const DoubleDouble product = twoProduct(centre, quotient);
    const DoubleDouble sum = twoSum(1.0, product.hi);
    const DoubleDouble denominatorSum =
        fastTwoSum(sum.hi, sum.lo + (product.lo + centre * quotientLow));
    const DoubleDouble reduced = divide(difference, denominatorSum);
    const DoubleDouble arctangentOfReduced = multiply(
        reduced, series(trig::kArctangentHead, trig::kArctangentTail, multiply(reduced, reduced)));
    const auto index = static_cast<std::ptrdiff_t>(eighths);  // 0 to 8
    return add(*std::next(trig::kArctangentOfEighths.begin(), index), arctangentOfReduced);
}

}  // namespace

SinCos sinCos(double angle) noexcept {
    assert(std::isfinite(angle) && std::fabs(angle) <= kMaxTrigArgument);
    if (std::fabs(angle) < kSmallAngle) {
        return {angle, 1.0};
    }
    const Reduction reduction = reduce(angle);
    const DoubleDouble r = reduction.angle;
    const DoubleDouble z = multiply(r, r);
    const double sine = multiply(r, series(trig::kSineHead, trig::kSineTail, z)).hi;
    const double cosine = series(trig::kCosineHead, trig::kCosineTail, z).hi;
    switch (reduction.quadrant) {
        case 0:
            return {sine, cosine};
        case 1:
            return {cosine, -sine};
        case 2:
            return {-sine, -cosine};
        default:
            return {-cosine, sine};
    }
}

double atan2(double y, double x) noexcept {
    assert(std::isfinite(y) && std::isfinite(x));
    if (y == 0.0) {
        return std::signbit(x) ? std::copysign(trig::kPi.hi, y) : y;
    }
    double numerator = std::fabs(y);
    double denominator = std::fabs(x);
    const bool swapped = numerator > denominator;
    if (swapped) {
        std::swap(numerator, denominator);
    }
    if (denominator < kSmallMagnitude) {
        numerator *= kScaleUp;
        denominator *= kScaleUp;
    }
    DoubleDouble angle = arctangent(numerator, denominator);
    if (swapped) {
        angle = add(trig::kPiOverTwo, negate(angle));
    }
    if (x < 0.0) {
        angle = add(trig::kPi, negate(angle));
    }
    return std::copysign(angle.hi, y);
}

}  // namespace solid_space::robust
