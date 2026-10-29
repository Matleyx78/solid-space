// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Draft contract (ADR 0007, point 15): see ADR 0009, the single rule for
// coincidence and tolerance in the plane. Declarations only: nothing here may
// be used before the package that implements it is merged.
//
// Two different questions are kept apart (ADR 0009, point 1):
// - what the data are: exact decisions (predicates.hpp, geom2d), which never
//   use a tolerance;
// - what the user meant: this rule, applied only by the functions that name
//   it. Its answer is used to change the data (merge two points, drop a
//   degenerate piece), never inside an exact decision.

#include "solid_space/robust/diagnosis.hpp"
#include "solid_space/robust/point.hpp"
#include "solid_space/robust/result.hpp"

namespace solid_space::robust {

/// The physical floor of the tolerance: one nanometre, in metres. No two
/// things closer than this are ever told apart, whatever the drawing.
inline constexpr double kPhysicalFloorMeters = 1e-9;

/// The relative part of the tolerance: 2^-42, about 2.3e-13. It is 64 times
/// the largest error of a point built by geom2d (2^-48, see
/// geom2d/curves.hpp), so a built point always coincides, under this rule,
/// with the exact point it stands for.
inline constexpr double kRelativeTolerance = 0x1p-42;

/// The tolerance rule of a drawing.
///
/// The tolerance for data of magnitude M (the largest absolute value among
/// the coordinates, centres and radii involved) is
///
///     epsilon(M) = max(floorLength(), kRelativeTolerance * M)
///
/// where floorLength() is kPhysicalFloorMeters expressed in drawing units
/// (kPhysicalFloorMeters / unitInMeters, rounded to double). It depends only
/// on the data being judged and on the unit, never on the rest of the
/// drawing. It is not transitive: a ~ b and b ~ c do not imply a ~ c.
class TolerancePolicy {
public:
    /// The rule for a drawing whose unit is unitInMeters metres long (0.001
    /// for millimetres, 1000 for kilometres). Mapping the units of a file
    /// format (for example INSUNITS of the DXF) to metres is up to the
    /// caller.
    ///
    /// Diagnosis invalidValue when unitInMeters is not finite, not positive,
    /// or gives a floor that is not a positive finite double of magnitude at
    /// most kMaxCoordinate.
    [[nodiscard]] static Result<TolerancePolicy, Diagnosis> forUnit(double unitInMeters);

    /// The length of one drawing unit, in metres.
    [[nodiscard]] double unitInMeters() const noexcept;

    /// kPhysicalFloorMeters in drawing units: the smallest tolerance.
    [[nodiscard]] double floorLength() const noexcept;

    /// epsilon(magnitude), as above, in drawing units.
    ///
    /// Precondition (asserted in debug): magnitude is finite, in
    /// [0, kMaxCoordinate].
    [[nodiscard]] double epsilon(double magnitude) const noexcept;

    /// True when the exact distance between a and b is at most epsilon(M),
    /// with M the largest absolute value among the four coordinates. The
    /// comparison is exact: no rounding decides it.
    ///
    /// Precondition (asserted in debug): every coordinate is finite, with
    /// magnitude at most kMaxCoordinate.
    [[nodiscard]] bool coincident(Point2 a, Point2 b) const noexcept;

    /// True when a length of the given value, measured on data of the given
    /// magnitude, is at most epsilon(magnitude): the length is negligible.
    ///
    /// Precondition (asserted in debug): both are finite, in
    /// [0, kMaxCoordinate].
    [[nodiscard]] bool negligible(double length, double magnitude) const noexcept;

private:
    TolerancePolicy(double unitInMeters, double floorLength) noexcept;

    double m_unitInMeters;
    double m_floorLength;
};

}  // namespace solid_space::robust
