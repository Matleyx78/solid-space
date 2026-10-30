// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Exact dyadic numbers: the arithmetic under every decision of geom2d (ADR 0010
// and ADR 0011, "Che cosa rende possibili decisioni esatte"; ADR 0008, point 6).
// Internal to geom2d: nothing here is part of the public contract.
//
// The data of geom2d range from 2^-1074 to 2^50 and the decisions evaluate
// products of degree up to about ten, so the arithmetic must be exact for every
// exponent: an integer of any size times a power of two. All the computation is
// in dyadic.cpp (ADR 0008, point 5): this header only declares.
//
// Memory: numbers of up to Natural::kInlineLimbs limbs (256 bits: a double has
// 53, the product of two doubles 106) live inside the object and allocate
// nothing. Larger numbers allocate, and std::bad_alloc is the only exception
// that may leave these functions (ADR 0003, point 5): the ones that may
// allocate are not noexcept.

#include "solid_space/robust/predicates.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace solid_space::geom2d::exact {

/// A natural number of any size, stored as 64-bit limbs, least significant
/// first, with no leading zero limb (zero has no limbs). It is only the
/// storage of Dyadic: the arithmetic on it lives in dyadic.cpp.
class Natural {
public:
    using Limb = std::uint64_t;

    /// Limbs stored inside the object, with no allocation.
    static constexpr std::size_t kInlineLimbs = 4;

    /// Zero.
    Natural() noexcept = default;

    /// The number `value`.
    explicit Natural(Limb value) noexcept;

    [[nodiscard]] bool isZero() const noexcept { return m_size == 0; }

    /// The number of limbs, without leading zero limbs.
    [[nodiscard]] std::size_t size() const noexcept { return m_size; }

    /// The limbs, least significant first: size() of them.
    [[nodiscard]] const Limb* data() const noexcept;

    /// Makes the number `size` zero limbs long and returns them for writing.
    /// The caller writes the value and then calls trim().
    [[nodiscard]] Limb* assignZeros(std::size_t size);

    /// Drops the leading zero limbs left by assignZeros.
    void trim() noexcept;

private:
    [[nodiscard]] Limb* mutableData() noexcept;

    std::size_t m_size = 0;
    std::array<Limb, kInlineLimbs> m_inline{};
    // Used only above kInlineLimbs limbs; empty otherwise, so copying a small
    // number never allocates.
    std::vector<Limb> m_heap;
};

/// An exact dyadic number: an integer of any size times a power of two. Every
/// finite double is one; sums, differences and products are exact for any
/// exponents, with no underflow and no overflow.
///
/// A zero remembers a sign only so that toDouble() can give -0.0 back: the
/// constructor keeps the sign of -0.0, and +, - and * give a zero the sign
/// that IEEE 754 gives it in round-to-nearest (x + (-x) is +0, -0 + -0 is -0,
/// the sign of a product is the exclusive or of the signs). sign() and
/// compare() do not see it: -0 and +0 are the same number.
class Dyadic {
public:
    /// Zero (+0).
    Dyadic() noexcept = default;

    /// The exact value of a double. Precondition (asserted in debug): `value`
    /// is finite. Subnormals and both zeros are accepted.
    explicit Dyadic(double value) noexcept;

    /// The exact value of an integer.
    [[nodiscard]] static Dyadic fromInteger(std::int64_t value) noexcept;

    /// The sign of the number (zero for -0 too).
    [[nodiscard]] robust::Sign sign() const noexcept;

    [[nodiscard]] bool isZero() const noexcept { return m_magnitude.isZero(); }

    /// The nearest double, ties to even, as IEEE 754 rounds the result of an
    /// operation: values beyond the largest double become +-infinity, values
    /// below half the smallest subnormal become a zero with the sign of the
    /// number.
    [[nodiscard]] double toDouble() const noexcept;

    /// The number times 2^power, exactly.
    [[nodiscard]] Dyadic timesPowerOfTwo(std::int64_t power) const;

    /// The absolute value (a zero becomes +0).
    [[nodiscard]] Dyadic abs() const;

    friend Dyadic operator+(const Dyadic& a, const Dyadic& b);
    friend Dyadic operator-(const Dyadic& a, const Dyadic& b);
    friend Dyadic operator-(const Dyadic& a);
    friend Dyadic operator*(const Dyadic& a, const Dyadic& b);

    Dyadic& operator+=(const Dyadic& other);
    Dyadic& operator-=(const Dyadic& other);
    Dyadic& operator*=(const Dyadic& other);

    /// The sign of a - b, computed without allocating.
    friend robust::Sign compare(const Dyadic& a, const Dyadic& b) noexcept;

    /// The nearest double to numerator / denominator, ties to even, rounded
    /// as toDouble() rounds. A zero numerator gives a zero with the sign of
    /// the quotient. Precondition (asserted in debug): `denominator` is not
    /// zero.
    friend double quotientToDouble(const Dyadic& numerator, const Dyadic& denominator);

    /// A lower bound of the square root of `value`, accurate to `bits` bits:
    /// S <= sqrt(value) < S * (1 + 2^(1 - bits)), and S is zero only when
    /// `value` is. Preconditions (asserted in debug): `value` is not negative,
    /// `bits` is at least 2.
    friend Dyadic floorSqrt(const Dyadic& value, std::int64_t bits);

private:
    Dyadic(Natural magnitude, std::int64_t exponent, bool negative);

    /// Moves the trailing zero bits of the magnitude into the exponent, so that
    /// every number has one representation: an odd magnitude, or zero with
    /// exponent zero.
    void normalize();

    // The value is (-1)^m_negative * m_magnitude * 2^m_exponent.
    Natural m_magnitude;
    std::int64_t m_exponent = 0;
    bool m_negative = false;
};

// The friends again at namespace scope, where [[nodiscard]] applies.
[[nodiscard]] Dyadic operator+(const Dyadic& a, const Dyadic& b);
[[nodiscard]] Dyadic operator-(const Dyadic& a, const Dyadic& b);
[[nodiscard]] Dyadic operator-(const Dyadic& a);
[[nodiscard]] Dyadic operator*(const Dyadic& a, const Dyadic& b);
[[nodiscard]] robust::Sign compare(const Dyadic& a, const Dyadic& b) noexcept;
[[nodiscard]] double quotientToDouble(const Dyadic& numerator, const Dyadic& denominator);
[[nodiscard]] Dyadic floorSqrt(const Dyadic& value, std::int64_t bits);

}  // namespace solid_space::geom2d::exact
