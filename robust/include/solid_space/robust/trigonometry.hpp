// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Draft contract (ADR 0007, point 15): see ADR 0008 (point 5) and ADR 0010.
// Declarations only: nothing here may be used before the package that
// implements it is merged.
//
// The standard library gives different last bits for sin, cos and atan2 on
// MSVC, glibc and macOS. These functions give the same bits everywhere,
// because they use only operations that IEEE 754 rounds correctly.

namespace solid_space::robust {

/// Largest magnitude of an angle accepted by sinCos, in radians: 2^20, about
/// 1.05e6. Drawings store angles in [0, 2*pi) or close to it.
inline constexpr double kMaxTrigArgument = 0x1p20;

/// Largest error of sinCos and atan2, in units in the last place of the exact
/// result.
inline constexpr double kTrigMaxErrorUlps = 2.0;

/// The sine and cosine of the same angle.
struct SinCos {
    double sine = 0.0;
    double cosine = 1.0;
};

/// Sine and cosine of an angle in radians, each within kTrigMaxErrorUlps of
/// the exact value, with the same bits on every platform.
///
/// Precondition (asserted in debug): angle is finite, with magnitude at most
/// kMaxTrigArgument.
[[nodiscard]] SinCos sinCos(double angle) noexcept;

/// The angle of the vector (x, y) from the positive x axis, counterclockwise,
/// in [-pi, pi] (pi being the double nearest to it), within
/// kTrigMaxErrorUlps of the exact value and with the same bits on every
/// platform. Signed zeros follow the C standard (atan2(+0, -1) is pi,
/// atan2(-0, -1) is -pi); atan2(0, 0) is 0.
///
/// Precondition (asserted in debug): x and y are finite.
[[nodiscard]] double atan2(double y, double x) noexcept;

}  // namespace solid_space::robust
