// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Draft contract (ADR 0007, point 15): see ADR 0008 (point 7), ADR 0009 and
// ADR 0010.

#include "solid_space/robust/result.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace solid_space::geom2d {

/// Why an operation of geom2d gave no value. The values are stable: new codes
/// are added at the end, existing ones never change meaning.
enum class DiagnosisCode : std::uint8_t {
    /// A value is NaN or infinite, or a radius is negative.
    invalidValue,
    /// A coordinate, radius or derived point has magnitude beyond
    /// robust::kMaxCoordinate, or an angle beyond robust::kMaxTrigArgument.
    outOfDomain,
    /// The curve is exactly a single point: a segment whose start equals its
    /// end, a radius of zero, an arc reduced to one direction (see Arc).
    degenerateCurve,
    /// The operation needs a circular curve (an arc, a circle or a curved
    /// polyline segment) and got a straight one.
    notCircular,
};

/// The diagnosis of geom2d (ADR 0003, point 5): a stable code, the input it
/// is about and a message for people.
struct Diagnosis {
    DiagnosisCode code = DiagnosisCode::invalidValue;
    /// Which input: 0 for the first curve (or the host of intersectAlong),
    /// 1 for the second, i + 1 for cutter i.
    std::size_t input = 0;
    std::string message;
};

/// The result of an operation of geom2d.
template <typename T>
using Result = robust::Result<T, Diagnosis>;

}  // namespace solid_space::geom2d
