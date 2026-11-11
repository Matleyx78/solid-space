// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Contract with Tecnigrafo (ADR 0007, point 15): see ADR 0016.
//
// tan, acos and hypot with the same bits on every platform, like sinCos and
// atan2 (trigonometry.hpp): the C and C++ standards do not ask the standard
// library to round them correctly, so MSVC, glibc and macOS may give
// different last bits. These functions are computed in robust's own source files, with
// only operations that IEEE 754 rounds correctly (ADR 0008, point 5).
// Correct rounding is not promised either: each result is within a declared
// error of the exact value of the function at the given doubles.
//
// Declared, not yet implemented: a function is used only after the package
// that implements it is merged (ADR 0007, point 15).

#include "solid_space/robust/trigonometry.hpp"  // kMaxTrigArgument

namespace solid_space::robust {

/// Largest error of tan, acos and hypot, in units in the last place of the
/// exact result. For a subnormal result the unit is 2^-1074.
inline constexpr double kElementaryMaxErrorUlps = 2.0;

/// Largest magnitude of each argument of hypot: 2^1023, about 9.0e307. The
/// exact length is then below 2^1023.5, so the result is always finite.
inline constexpr double kMaxHypotArgument = 0x1p1023;

/// The tangent of an angle in radians, within kElementaryMaxErrorUlps of the
/// exact tangent of the given double, with the same bits on every platform.
///
/// No double is a pole of the tangent: for every accepted angle the result is
/// finite, with magnitude below 2^61 (the doubles up to kMaxTrigArgument stay
/// at least 2^-60.49 away from every odd multiple of pi/2; see
/// trigonometry.cpp, sinCos, step 1). Near such a multiple the tangent
/// changes fast with its argument: the error is relative to the tangent of
/// the double given, not of the angle the caller had in mind.
///
/// tan(-angle) is -tan(angle) bit for bit; tan(+-0) is +-0.
///
/// Precondition (asserted in debug): angle is finite, with magnitude at most
/// kMaxTrigArgument (the limit of sinCos).
[[nodiscard]] double tan(double angle) noexcept;

/// The arccosine of x, in radians, in [0, pi] (pi being the double nearest to
/// it), within kElementaryMaxErrorUlps of the exact value, with the same bits
/// on every platform. The result is never below +0 nor above the double
/// nearest to pi. acos(1) is +0 and acos(-1) is the double nearest to pi,
/// exactly.
///
/// Precondition (asserted in debug): x is finite and -1 <= x <= 1. A cosine
/// computed as a / hypot(a, b) always satisfies it (see hypot).
[[nodiscard]] double acos(double x) noexcept;

/// The length of the vector (x, y), the square root of x^2 + y^2, within
/// kElementaryMaxErrorUlps of the exact value, with the same bits on every
/// platform. No intermediate value overflows or underflows: x and y are taken
/// as they are, subnormals included.
///
/// - When the exact length is a double, it is returned exactly: hypot(3, 4)
///   is 5, and hypot(x, +-0) is |x|.
/// - The result is never below the larger of |x| and |y|: unless x and y
///   are both zero, |x| / hypot(x, y) is never above 1.
/// - The result depends only on |x| and |y| as an unordered pair:
///   hypot(x, y), hypot(y, x), hypot(-x, y) and hypot(x, -y) have the same
///   bits.
/// - hypot(+-0, +-0) is +0; any other result is positive.
///
/// Precondition (asserted in debug): x and y are finite, with magnitude at
/// most kMaxHypotArgument.
[[nodiscard]] double hypot(double x, double y) noexcept;

}  // namespace solid_space::robust
