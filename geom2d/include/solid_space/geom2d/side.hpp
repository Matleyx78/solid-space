// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Draft contract (ADR 0007, point 15): see ADR 0010, request #20.
// Declarations only: nothing here may be used before the package that
// implements it is merged.

#include "solid_space/geom2d/curves.hpp"
#include "solid_space/geom2d/diagnosis.hpp"

#include <cstdint>

namespace solid_space::geom2d {

/// Where a point lies with respect to the line or circle that carries a
/// curve, looking along the direction of the curve.
enum class Side : std::uint8_t {
    left,
    on,
    right,
};

/// The side of a point with respect to the whole line or circle of a curve,
/// decided exactly, with no tolerance:
/// - Segment, straight PolylineSegment: left or right of the line directed
///   from start to end, on when the three points are collinear;
/// - Circle: left is inside, right is outside (a circle runs
///   counterclockwise);
/// - Arc, curved PolylineSegment: the side of their circle in the direction
///   of the curve, so inside is left for a counterclockwise curve and right
///   for a clockwise one.
/// It agrees with intersect(): a point is on exactly when it lies on the
/// extended curve.
///
/// Diagnosis invalidValue, outOfDomain or degenerateCurve about the curve
/// (input 0); invalidValue or outOfDomain about the point (input 1).
[[nodiscard]] Result<Side> side(const Curve& curve, Point2 point);

}  // namespace solid_space::geom2d
