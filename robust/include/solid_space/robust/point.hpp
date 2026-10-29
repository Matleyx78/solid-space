// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Draft contract (ADR 0007, point 15): see ADR 0009 and ADR 0010. Nothing
// declared in this header may be used before the package that implements it
// is merged.

namespace solid_space::robust {

/// Largest magnitude accepted for a coordinate, a radius or any other length
/// given to robust and geom2d: 2^50, about 1.1e15. It leaves room for the
/// products that exact decisions evaluate, and it is far beyond any drawing:
/// a kilometre drawn in nanometres is 1e12 units.
inline constexpr double kMaxCoordinate = 0x1p50;

/// A point, or a vector, of the plane. The two values are taken exactly as
/// they are: every decision about a point is a decision about these doubles.
struct Point2 {
    double x = 0.0;
    double y = 0.0;
};

}  // namespace solid_space::robust
