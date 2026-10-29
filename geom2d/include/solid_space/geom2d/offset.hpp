// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Draft contract (ADR 0007, point 15): see ADR 0012, request #22.
// Declarations only: nothing here may be used before the package that
// implements it is merged.
//
// The parallel curve (offset) of a curve or of a polyline, as OFFSET of
// AutoCAD with OFFSETGAPTYPE = 0.
//
// The distance is signed: positive to the left of the curve, negative to its
// right, looking along its direction (side.hpp). For a circle, and for a
// counterclockwise arc, left is inside. A distance of zero gives the input
// itself. The caller turns a side point into a sign with side().
//
// The magnitude M of an offset is the largest among the magnitude of the
// input (curves.hpp; for a polyline, of all its pieces) and |distance|.

#include "solid_space/geom2d/curves.hpp"
#include "solid_space/geom2d/diagnosis.hpp"
#include "solid_space/robust/tolerance.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace solid_space::geom2d {

/// Largest distance between a returned offset and the exact one, relative to
/// the magnitude: every point of the returned curve or polyline lies within
/// kRelativeOffsetError * max(M, |p|) of the exact offset, and every point of
/// the exact offset within the same distance of the returned one. 2^-46 is
/// about 1.4e-14, 16 times below the relative tolerance of ADR 0009.
inline constexpr double kRelativeOffsetError = 0x1p-46;

/// How an offset ended.
enum class OffsetOutcome : std::uint8_t {
    /// There is a result.
    done,
    /// Nothing is left at that distance: an arc or a circle whose radius would
    /// be zero or negative, a polyline whose pieces all vanish.
    vanished,
    /// A polyline offset that could not be made without self-intersections
    /// (see PolylineOffset::failure). Never for a single curve.
    failed,
};

/// The offset of one curve.
struct CurveOffset {
    /// done or vanished.
    OffsetOutcome outcome = OffsetOutcome::vanished;
    /// With done, the same kind of curve as the input:
    /// - Segment and straight PolylineSegment: moved by |distance| along the
    ///   normal; each end within kRelativePointError;
    /// - Circle: the same centre, radius R - distance (rounded to double);
    /// - Arc: the same centre, start angle and sweep, so the same exact
    ///   directions D0 and D1 (curves.hpp); radius R - distance for a
    ///   counterclockwise arc, R + distance for a clockwise one (rounded);
    /// - curved PolylineSegment: the same bulge, so the same included angle;
    ///   start and end moved along the radius, each within
    ///   kRelativePointError.
    /// The new radius of a circular curve is compared with zero exactly: zero
    /// or less gives vanished.
    Curve curve;
};

/// The offset of one curve at a signed distance.
///
/// Diagnosis invalidValue, outOfDomain or degenerateCurve about the curve
/// (input 0); invalidValue when the distance is not finite, outOfDomain when
/// its magnitude is beyond robust::kMaxCoordinate or a point of the result
/// would be (input 1).
[[nodiscard]] Result<CurveOffset> offsetCurve(const Curve& curve, double distance);

/// Why a polyline offset failed.
enum class OffsetFailure : std::uint8_t {
    /// It did not fail.
    none,
    /// Two pieces left next to each other, after pieces between them vanished
    /// or were removed, do not meet.
    openJoint,
    /// A joint would lie beyond robust::kMaxCoordinate.
    beyondDomain,
    /// The result would intersect itself.
    selfIntersection,
    /// The result would meet the original polyline.
    meetsOriginal,
};

/// Where a piece of the result comes from.
enum class PieceOrigin : std::uint8_t {
    /// The offset of a piece of the input, cut or extended at its joints.
    source,
    /// An arc that closes a joint, centred on a vertex of the input.
    gap,
};

/// The origin of one piece of the result.
struct OffsetPiece {
    PieceOrigin origin = PieceOrigin::source;
    /// source: the index of the piece in pieces(input); gap: the index of the
    /// vertex of the input that the arc turns around.
    std::size_t index = 0;
};

/// The offset of a polyline.
struct PolylineOffset {
    OffsetOutcome outcome = OffsetOutcome::failed;
    /// With failed, why; otherwise none.
    OffsetFailure failure = OffsetFailure::none;
    /// True when no piece vanished or was removed (steps 1 and 3 of
    /// offsetPolyline).
    bool simple = false;
    /// With done: open or closed as the input.
    Polyline polyline;
    /// With done: one entry per piece of the result, in order, so that the
    /// caller can carry over what belongs to each source piece (its width).
    std::vector<OffsetPiece> origins;
};

/// OFFSET of a polyline (AutoCAD, OFFSETGAPTYPE = 0) at a signed distance.
///
/// The exact result is defined by these steps on the exact values. Pieces of
/// the input that are exactly a single point are dropped first (a polyline
/// often repeats a vertex); they do not count as vanished.
/// 1. Each piece is offset as offsetCurve does. A curved piece whose radius
///    becomes zero or negative vanishes.
/// 2. Joints. Two offset pieces A and B left next to each other (in a closed
///    polyline, also the last and the first) whose ends differ are joined at
///    a vertex V of the input: the vertex between their source pieces, or,
///    when pieces between them vanished or were removed, the vertex at the end
///    of the source piece of A.
///    - If their extended curves meet, they are joined at the common point
///      nearest V (compared exactly; a tie goes to the first along A): A is
///      cut or extended to end there, B to start there.
///    - Otherwise, if their source pieces are consecutive, by an arc of radius
///      |distance| centred on V, from the end of A to the start of B, turning
///      as the input turns at V. Both ends lie exactly at |distance| from V.
///    - Otherwise the offset fails with openJoint: no such arc ends on both.
/// 3. A piece that its joints turn backwards (its new end lies before its new
///    start along it, or an arc whose sweep would change sign) is removed, and
///    its neighbours are joined again as in step 2. Steps 2 and 3 repeat until
///    no piece is removed.
/// 4. Pieces of the result that are exactly a single point are dropped.
///
/// The result is checked on its own doubles before it is returned, so that
/// done always means:
/// - no two pieces that are not consecutive have a common point (decided
///   exactly, as intersect() does);
/// - two consecutive pieces meet only at their shared vertex, or at points
///   that coincide with it under the rule of ADR 0009 (`policy`), as a line
///   and an arc tangent at a vertex may after rounding;
/// - the result does not meet the original polyline.
/// Otherwise the outcome is failed, with the reason. A result is never wrong
/// in silence.
///
/// What is guaranteed:
/// - simple cases: when no piece vanishes or is removed and the exact result
///   stays farther than epsilon(M) (ADR 0009) from itself (pieces not
///   consecutive) and from the input, the outcome is done, simple is true,
///   and the result is the exact one: every vertex within
///   kRelativePointError and every point within kRelativeOffsetError;
/// - the other cases: done, with the result of the steps above within the same
///   errors, or failed. They may differ from AutoCAD.
///
/// Diagnosis invalidValue or outOfDomain about the polyline (input 0, the
/// message names the vertex); degenerateCurve when no piece is left after
/// the first drop (input 0); invalidValue when the distance is not finite,
/// outOfDomain when its magnitude is beyond robust::kMaxCoordinate (input 1).
[[nodiscard]] Result<PolylineOffset> offsetPolyline(const Polyline& polyline, double distance,
                                                    const robust::TolerancePolicy& policy);

}  // namespace solid_space::geom2d
