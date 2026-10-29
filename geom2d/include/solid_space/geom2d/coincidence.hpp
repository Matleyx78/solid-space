// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Draft contract (ADR 0007, point 15): see ADR 0009, request #23. The rule
// of robust/tolerance.hpp applied to curves. Declarations only: nothing here
// may be used before the package that implements it is merged.
//
// Every answer here is "what the user meant", not "what the data are": it is
// used to change the data (join two entities, drop a degenerate piece, set a
// bulge to zero), never inside an exact decision. Each comparison with the
// tolerance is exact. M is the magnitude of all the curves involved
// (curves.hpp), and epsilon is policy.epsilon(M).
//
// Each function checks its curves for invalidValue and outOfDomain
// (curves.hpp, "Checking the data") and gives that diagnosis. Exactly
// degenerate curves are accepted where the rule makes sense for them
// (isDegenerate, isStraight) and give degenerateCurve elsewhere.

#include "solid_space/geom2d/curves.hpp"
#include "solid_space/geom2d/diagnosis.hpp"
#include "solid_space/robust/tolerance.hpp"

namespace solid_space::geom2d {

/// True when every two points of the curve are within epsilon of each other:
/// the length for a segment, the chord for an arc up to half a turn, the
/// diameter for a larger arc and for a circle. An exactly degenerate curve is
/// degenerate here too.
[[nodiscard]] Result<bool> isDegenerate(const Curve& curve, const robust::TolerancePolicy& policy);

/// True when the arc of a polyline segment strays from its chord by at most
/// epsilon (its sagitta, |bulge| * chord / 2, is at most epsilon), so that its
/// bulge counts as zero. Always true when bulge == 0.
[[nodiscard]] Result<bool> isStraight(const PolylineSegment& segment,
                                      const robust::TolerancePolicy& policy);

/// True when two segments lie on the same line: each end of each segment is
/// within epsilon of the line of the other. Symmetric.
///
/// Diagnosis degenerateCurve if either segment is exactly a point.
[[nodiscard]] Result<bool> areAligned(const Segment& first, const Segment& second,
                                      const robust::TolerancePolicy& policy);

/// True when two circular curves (Arc, Circle, curved PolylineSegment) have
/// centres within epsilon of each other. Symmetric.
///
/// Diagnosis notCircular if either curve is straight; degenerateCurve if
/// either is exactly a point.
[[nodiscard]] Result<bool> areConcentric(const Curve& first, const Curve& second,
                                         const robust::TolerancePolicy& policy);

/// True when two circular curves lie on the same circle: they are concentric
/// and their radii differ by at most epsilon. Symmetric.
///
/// Diagnoses as areConcentric.
[[nodiscard]] Result<bool> onSameCircle(const Curve& first, const Curve& second,
                                        const robust::TolerancePolicy& policy);

}  // namespace solid_space::geom2d
