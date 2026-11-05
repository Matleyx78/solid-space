// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Contract with Tecnigrafo (ADR 0007, point 15): see ADR 0009, request #23. The rule
// of robust/tolerance.hpp applied to curves.
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
//
// joinAligned uses no tolerance: it orders exactly the ends of segments that
// the rule has already found aligned (decision of 2026-11-02, issue #158).

#include "solid_space/geom2d/curves.hpp"
#include "solid_space/geom2d/diagnosis.hpp"
#include "solid_space/robust/tolerance.hpp"

#include <span>

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

/// The segment that joins a source segment with segments aligned with it (as
/// areAligned decides), as JOIN needs it: from the first to the last of all
/// their ends along the source, gaps included.
///
/// The ends are, in this order, the start and the end of source, then the
/// start and the end of each segment of others, in the order of others.
/// Along the source, with d = source.end - source.start, an end p comes
/// before an end q when (p - source.start) . d is smaller for p than for q,
/// decided exactly. The result starts at the first end along the source and
/// ends at the last one; among ends with the same exact value, the first in
/// the order above is taken, for the start as for the end. So an end of
/// source wins every tie, and an end of others[i] one with others[j], j > i.
///
/// Both points are input doubles, returned unchanged: nothing is constructed
/// or rounded, and they keep the magnitude of the inputs. The result runs in
/// the direction of source and is never degenerate (its start is at most 0
/// along the source and its end at least d . d > 0). With no others, it is
/// source itself. Reversing source gives the same result reversed, bit for
/// bit; reordering others can change only which end wins a tie.
///
/// No tolerance is used and the alignment is not checked: for any segments
/// the answer is the one above, and it is the joined line only when every
/// segment of others is aligned with source.
///
/// Diagnosis invalidValue, outOfDomain or degenerateCurve about source
/// (input 0) or about others[i] (input i + 1), checked in input order
/// (curves.hpp, "Checking the data"): one bad segment fails the call.
[[nodiscard]] Result<Segment> joinAligned(const Segment& source, std::span<const Segment> others);

}  // namespace solid_space::geom2d
