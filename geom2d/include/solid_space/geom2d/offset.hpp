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
// counterclockwise arc, left is inside. A distance of zero gives the curve
// itself from offsetCurve; offsetPolyline fails with meetsOriginal, because
// its result would be the input. The caller turns a side point into a sign
// with side().
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
/// kRelativeOffsetError * max(M, |p|, 2^-1022) of the exact offset, and every point of
/// the exact offset within the same distance of the returned one. 2^-46 is
/// about 1.4e-14, 16 times below the relative tolerance of ADR 0009.
inline constexpr double kRelativeOffsetError = 0x1p-46;

/// How an offset ended.
enum class OffsetOutcome : std::uint8_t {
    /// There is a result.
    done,
    /// Nothing is left at that distance: an arc or a circle whose radius would
    /// be zero or negative, a polyline whose pieces all vanish, were removed or
    /// reduced to a single point.
    vanished,
    /// An offset that could not be made: a polyline offset without
    /// self-intersections (see PolylineOffset::failure), or an arc of the
    /// result that its rounded ends and a bulge cannot represent within
    /// kRelativeOffsetError (see CurveOffset). For a single curve, only a
    /// curved PolylineSegment.
    failed,
};

/// The offset of one curve. Every returned vertex (the ends of a Segment or
/// PolylineSegment, straight or curved) is rounded coordinate by coordinate
/// to the nearest double, ties to even. Bulges and representability checks
/// use these doubles, not arbitrary points within the error bound.
struct CurveOffset {
    /// done or vanished; failed only for a curved PolylineSegment (below).
    OffsetOutcome outcome = OffsetOutcome::vanished;
    /// With done, the same kind of curve as the input:
    /// - Segment and straight PolylineSegment: moved by |distance| along the
    ///   normal; each end within kRelativePointError;
    /// - Circle: the same centre, radius R - distance (rounded to double);
    /// - Arc: the same centre, start angle and sweep, so the same exact
    ///   directions D0 and D1 (curves.hpp); radius R - distance for a
    ///   counterclockwise arc, R + distance for a clockwise one (rounded);
    /// - curved PolylineSegment: start and end moved along the radius, each
    ///   within kRelativePointError; the bulge is that of the arc from the
    ///   returned start to the returned end, turning as the input, whose
    ///   centre is the point of their perpendicular bisector nearest the exact
    ///   centre C of the offset circle. With m the middle of the returned
    ///   ends, n the vector from start to end turned a quarter turn
    ///   counterclockwise and t = ((C - m) . n) / (n . n), all exact, the
    ///   bulge is the double nearest to -2 t + sqrt(4 t^2 + 1) for a
    ///   counterclockwise piece and to -2 t - sqrt(4 t^2 + 1) for a clockwise
    ///   one. So the arc keeps the exact centre as well as the rounded ends
    ///   allow, and its included angle may differ from that of the input by
    ///   the rounding of the ends. If the rounded ends coincide, the outcome
    ///   is failed with no curve: a nonzero exact arc must not become a point.
    /// The new radius of a circular curve is compared with zero exactly: zero
    /// or less gives vanished.
    ///
    /// failed, with no curve, when the arc of a curved PolylineSegment so
    /// built has a point farther than kRelativeOffsetError * max(M, |p|, 2^-1022)
    /// from the exact offset, or the exact offset a point that far from it:
    /// its rounded ends turn the chord too much for any bulge. For distinct
    /// rounded ends, never when both distances are within half that bound;
    /// it happens only for an arc near a full turn whose chord spans a few ulp
    /// of its coordinates. Coincident rounded ends always give failed.
    Curve curve;
};

/// The offset of one curve at a signed distance: done or vanished, or failed
/// for a curved PolylineSegment that cannot be represented (see CurveOffset).
///
/// Diagnosis invalidValue, outOfDomain or degenerateCurve about the curve
/// (input 0); invalidValue when the distance is not finite, outOfDomain when
/// its magnitude is beyond robust::kMaxCoordinate or the constructed curve
/// would not be valid data under curves.hpp (a coordinate, radius or the
/// reconstructed point m beyond the domain; input 1). The geometric extent
/// is not checked. At zero distance the validated input is returned with
/// identical bits and outcome done.
[[nodiscard]] Result<CurveOffset> offsetCurve(const Curve& curve, double distance);

/// Why a polyline offset failed.
enum class OffsetFailure : std::uint8_t {
    /// It did not fail.
    none,
    /// Two pieces left next to each other, after pieces between them vanished
    /// or were removed, do not meet: their extended curves have no common
    /// point, or overlap (the same line or the same circle). In a closed
    /// polyline, also when only one piece is left.
    openJoint,
    /// A data value of the final result (a vertex or the point m of a curved
    /// piece) would lie beyond robust::kMaxCoordinate.
    beyondDomain,
    /// The result would intersect itself.
    selfIntersection,
    /// The result would meet the original polyline.
    meetsOriginal,
    /// An arc of the result cannot be represented within kRelativeOffsetError
    /// by its rounded ends and a bulge (see CurveOffset and offsetPolyline).
    notRepresentable,
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
    /// vertex of the input that the arc turns around (when pieces between them
    /// vanished, were removed or were dropped as duplicate points, the vertex at
    /// the end of the source piece of A).
    std::size_t index = 0;
};

/// The offset of a polyline.
struct PolylineOffset {
    OffsetOutcome outcome = OffsetOutcome::failed;
    /// With failed, why; otherwise none.
    OffsetFailure failure = OffsetFailure::none;
    /// True when no piece vanished, was removed or reduced to a point (steps 1,
    /// 3 and 4 of offsetPolyline).
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
/// 1. Each piece is offset geometrically as offsetCurve does, using exact
///    intermediate values, without inheriting its constructed-result domain
///    diagnosis. A curved piece whose radius becomes zero or negative vanishes.
/// 2. Joints. Two offset pieces A and B left next to each other (in a closed
///    polyline, also the last and the first; if only one piece is left in a
///    closed polyline, the offset fails with openJoint) whose ends differ are
///    joined at a vertex V of the input: the vertex between their source
///    pieces, or, when pieces between them vanished, were removed or were
///    dropped as duplicate points, the vertex at the end of the source piece
///    of A.
///    - If their extended curves meet, that is, have isolated common points
///      (curves that overlap, the same line or the same circle, do not meet),
///      they are joined at the common point nearest V (compared exactly; a tie
///      goes to the first along A): A is cut or extended to end there, B to
///      start there. For an arc piece, the joint point is reached by the
///      shortest rotation from the moved end (extending forward, shortening
///      backward); at exactly half a turn, or with an included angle of a full
///      turn or more, the offset fails with selfIntersection.
///    - Otherwise, if their source pieces are consecutive, by an arc of radius
///      |distance| centred on V, from the end of A to the start of B, turning
///      as the input turns at V (by the tangents of the two source pieces
///      there). Both ends lie exactly at |distance| from V. When those
///      tangents are exactly opposite the turn has no sign: the arc is then the
///      half circle that continues the offset pieces tangentially, clockwise
///      for a positive distance and counterclockwise for a negative one.
///    - Otherwise the offset fails with openJoint: no such arc ends on both.
/// 3. A piece that its joints turn backwards (its new end lies before its new
///    start along it, or an arc whose sweep would change sign) is removed, and
///    its neighbours are joined again as in step 2. If multiple pieces turn
///    backwards in the same round, they are removed together before renewing
///    the joints. Removing a piece never restores the end that a joint gave to
///    its neighbour: an end left without a neighbour, the free end of an open
///    polyline, keeps its cut, and only an end that was never cut stays where
///    step 1 put it. Steps 2 and 3 repeat until no piece is removed.
/// 4. Pieces of the result that are exactly a single point are dropped (making
///    simple false). If no piece is left after steps 1, 3 and 4, the outcome is
///    vanished.
///
/// Every final vertex, including moved, cut or extended ends, joint points
/// and gap-arc ends, is rounded coordinate by coordinate to the nearest double,
/// ties to even. Shared vertices use those same doubles for both pieces.
/// Every arc of the result, a source piece (cut or not) or a gap arc, gets
/// its bulge by the rule of CurveOffset for a curved PolylineSegment, around
/// the exact centre of its circle (the centre of the source piece, or the
/// vertex V of a gap arc), with the same bound: if one cannot be represented,
/// the outcome is failed with notRepresentable. This is decided when the
/// result is assembled, before the checks below. In particular, coincident
/// rounded ends of a nonzero exact arc give failed with notRepresentable;
/// that arc is not dropped as a point by step 4.
///
/// The result is checked on its own doubles before it is returned, so that
/// done always means:
/// - the final vertices and the reconstructed point m of every curved piece
///   are within robust::kMaxCoordinate; otherwise failed with beyondDomain.
///   Only the final result after steps 1-4 is checked, not an intermediate
///   offset or joint later cut or removed;
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
/// - simple cases: when no piece vanishes, is removed or reduced to a point
///   and the exact result stays farther than epsilon(M) (ADR 0009) from itself
///   (pieces not consecutive) and from the input, and the exact result rounded
///   as above is valid data within the domain, the outcome is done, simple
///   is true, and the result is the exact one: every vertex within
///   kRelativePointError and every point within kRelativeOffsetError; or
///   failed with notRepresentable, under the condition of CurveOffset (an
///   arc near a full turn whose chord spans a few ulp, or coincident rounded
///   ends of a nonzero exact arc);
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
