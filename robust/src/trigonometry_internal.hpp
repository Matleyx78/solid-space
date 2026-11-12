// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

#include "trigonometry_constants.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>

namespace solid_space::robust::trig {

/// Below this magnitude sinCos returns (x, 1) (trigonometry.cpp, sinCos step 0).
constexpr double kSmallAngle = 0x1p-27;

/// Below this ratio atan(t) is t (atan2, step 1).
constexpr double kSmallRatio = 0x1p-27;

/// The centres of the arctangent reduction are multiples of 1/kEighths.
constexpr double kEighths = 8.0;

/// a + b exactly, with no condition on a and b.
inline DoubleDouble twoSum(double a, double b) noexcept {
    const double sum = a + b;
    const double bVirtual = sum - a;
    const double aVirtual = sum - bVirtual;
    return {sum, (a - aVirtual) + (b - bVirtual)};
}

/// a + b exactly, when a = 0 or |a| >= |b|.
inline DoubleDouble fastTwoSum(double a, double b) noexcept {
    const double sum = a + b;
    return {sum, b - (sum - a)};
}

/// a * b exactly when its product and residual do not underflow.
inline DoubleDouble twoProduct(double a, double b) noexcept {
    const double product = a * b;
    return {product, std::fma(a, b, -product)};
}

inline DoubleDouble negate(DoubleDouble a) noexcept {
    return {-a.hi, -a.lo};
}

inline DoubleDouble add(DoubleDouble a, DoubleDouble b) noexcept {
    const DoubleDouble high = twoSum(a.hi, b.hi);
    const DoubleDouble low = twoSum(a.lo, b.lo);
    const DoubleDouble partial = fastTwoSum(high.hi, high.lo + low.hi);
    return fastTwoSum(partial.hi, partial.lo + low.lo);
}

inline DoubleDouble multiply(DoubleDouble a, DoubleDouble b) noexcept {
    const DoubleDouble product = twoProduct(a.hi, b.hi);
    const double low = std::fma(a.lo, b.hi, std::fma(a.hi, b.lo, product.lo));
    return fastTwoSum(product.hi, low);
}

inline DoubleDouble divide(DoubleDouble a, DoubleDouble b) noexcept {
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

inline Reduction reduce(double x) noexcept {
    const double quotient = std::round(x * kTwoOverPi);
    const auto& parts = kPiOverTwoParts;
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
inline DoubleDouble arctangent(double numerator, double denominator) noexcept {
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
    const DoubleDouble arctangentOfReduced =
        multiply(reduced, series(kArctangentHead, kArctangentTail, multiply(reduced, reduced)));
    const auto index = static_cast<std::ptrdiff_t>(eighths);  // 0 to 8
    return add(*std::next(kArctangentOfEighths.begin(), index), arctangentOfReduced);
}

}  // namespace solid_space::robust::trig
