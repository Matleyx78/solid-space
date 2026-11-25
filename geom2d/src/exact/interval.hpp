// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// The floating-point filter in front of the exact signs (ADR 0010, "Che cosa
// rende possibili decisioni esatte" and point 8; stage 0.3 corrections,
// package FS2). Internal to geom2d: nothing here is part of the public
// contract.
//
// A sign is first computed in double, with a bound of the distance between the
// computed value and the exact one; the exact numbers of dyadic.hpp and
// quadratic.hpp compute it only when the bound does not decide. The filter
// answers positive or negative, and only when the whole interval of the result
// lies on one side of zero: it never answers zero. So, whenever it answers,
// its answer is the exact one, and nothing but the time depends on it: a
// system where it gives up where another one answers gives the same results
// (ADR 0008, point 5).
//
// Interval: an approximation `centre` of an exact real number X with a bound
// `radius` of its error, |X - centre| <= radius, so X lies in
// [centre - radius, centre + radius]. The operations below take intervals of
// exact numbers and give an interval of the exact result: every radius covers
// the error carried by the operands and the rounding of the operation, and is
// itself computed so that its own roundings only make it larger. The proofs
// are in interval.cpp, next to each operation. Only operations that IEEE 754
// rounds correctly are used: sum, difference, product, quotient, std::sqrt,
// std::fabs and comparisons, in round to nearest with no contraction
// (ADR 0008, point 5; cmake/SolidSpaceDeterminism.cmake).
//
// An interval whose radius is not finite (or is NaN) says nothing about its
// number: certainSign() never decides it, and every operation on it gives
// such an interval again. Overflow ends there: a centre that overflows makes
// the radius infinite.
//
// All the computation is in interval.cpp (ADR 0008, point 5): this header
// only declares. The functions allocate nothing.

#include "dyadic.hpp"
#include "solid_space/robust/predicates.hpp"

#include <cstdint>
#include <optional>

namespace solid_space::geom2d::exact {

/// An approximation of an exact real number X: |X - centre| <= radius. A
/// radius that is not finite says nothing.
struct Interval {
    double centre = 0.0;
    double radius = 0.0;
};

/// The interval of nothing: radius +infinity.
[[nodiscard]] Interval unknownInterval() noexcept;

/// True when the radius is finite (and so is the centre).
[[nodiscard]] bool isKnown(const Interval& value) noexcept;

/// The exact value of a double: radius zero. Precondition: `value` is finite.
[[nodiscard]] Interval intervalOf(double value) noexcept;

/// The nearest double to a dyadic number, with the bound of its rounding. A
/// number beyond the doubles gives an interval of nothing.
[[nodiscard]] Interval intervalOf(const Dyadic& value) noexcept;

[[nodiscard]] Interval operator+(const Interval& a, const Interval& b) noexcept;
[[nodiscard]] Interval operator-(const Interval& a, const Interval& b) noexcept;
[[nodiscard]] Interval operator-(const Interval& a) noexcept;
[[nodiscard]] Interval operator*(const Interval& a, const Interval& b) noexcept;

/// The quotient of two exact numbers. An interval of nothing when the interval
/// of the denominator contains zero, or when the quotient may be subnormal.
[[nodiscard]] Interval quotient(const Interval& numerator, const Interval& denominator) noexcept;

/// The square root of an exact number that is not negative (precondition: the
/// exact number, not the interval). An interval of nothing when the centre is
/// not positive.
[[nodiscard]] Interval squareRoot(const Interval& radicand) noexcept;

/// The sign of the exact number when the interval decides it: positive or
/// negative, never zero; nothing when the interval contains zero or says
/// nothing.
[[nodiscard]] std::optional<robust::Sign> certainSign(const Interval& value) noexcept;

/// Whether the filters of geom2d try the intervals before the exact signs: on
/// by default. Only the tests turn it off, to reach the exact path that the
/// filters fall back to and compare its answers with the filtered ones (they
/// must be the same). The switch is an atomic flag, read with relaxed order:
/// turning it off changes no answer, only the time.
[[nodiscard]] bool filterEnabled() noexcept;

/// Turns the filters on or off (see filterEnabled()).
void enableFilter(bool enabled) noexcept;

/// How the filtered functions answered while counting was on: from the
/// intervals, or from the exact path because the intervals did not decide.
struct FilterCounts {
    std::uint64_t decided = 0;
    std::uint64_t undecided = 0;
};

/// Starts counting the answers of the filtered functions from zero. Only for
/// the tests, which check how often the filter decides: counting changes no
/// answer.
void startFilterCount() noexcept;

/// Stops counting and returns the counts since startFilterCount().
[[nodiscard]] FilterCounts stopFilterCount() noexcept;

/// Called by each filtered function, once per call with the filter on: true
/// when the intervals decided. Does nothing unless counting is on.
void recordFilterAnswer(bool decided) noexcept;

}  // namespace solid_space::geom2d::exact
