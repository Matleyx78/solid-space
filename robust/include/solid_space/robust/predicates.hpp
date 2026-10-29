// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Draft contract (ADR 0007, point 15): see ADR 0010. Declarations only:
// nothing here may be used before the package that implements it is merged.

#include "solid_space/robust/point.hpp"

#include <cstdint>

namespace solid_space::robust {

/// The sign of an exact quantity.
enum class Sign : std::int8_t {
    negative = -1,
    zero = 0,
    positive = 1,
};

/// Orientation of the triangle (a, b, c): positive when c lies to the left of
/// the directed line from a to b (the triangle runs counterclockwise),
/// negative when it lies to the right, zero when the three points are
/// collinear (or two of them coincide).
///
/// Exact: the result is the sign of the exact determinant of the given
/// doubles, with no tolerance, and it is the same on every platform.
///
/// Precondition (asserted in debug): every coordinate is finite, with
/// magnitude at most kMaxCoordinate.
[[nodiscard]] Sign orient2d(Point2 a, Point2 b, Point2 c) noexcept;

}  // namespace solid_space::robust
