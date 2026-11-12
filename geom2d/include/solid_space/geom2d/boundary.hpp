// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Contract with Tecnigrafo (ADR 0007, point 15): see ADR 0014, request #214.
//
// The boundary of a region for a hatch, in the two ways HATCH of AutoCAD
// finds it:
// - detectBoundary: from a point picked by the user, the smallest closed
//   contour around it among the curves of the drawing, with the islands
//   inside (Pick internal point);
// - nestContours: from contours chosen by the user, which lie inside which
//   (Select objects).
// Choosing the candidate curves, units and the hatch styles stay with the
// caller: with the normal style all the loops count (even-odd rule,
// region.hpp), with "outer" those of depth 0 and 1, with "ignore" depth 0.
//
// What is exact and what is rounded. Every decision is exact: which curves
// meet and where, the face around the picked point, which pieces form each
// loop and in which order, the depths, the outcome. The tolerance of ADR 0009
// is used only to change the data before the decisions (steps 2, 3 and 4 of
// detectBoundary), as that ADR allows, never inside a decision. The points
// and bulges of the returned loops are rounded within the errors of
// region.hpp; which double is returned is not fixed (ADR 0013, point 1).
// Rounded loops that touch may cross by up to those errors, and two distinct
// vertices may round to the same point: the origins and the order of the
// pieces are exact and stay as they are.

#include "solid_space/geom2d/curves.hpp"
#include "solid_space/geom2d/diagnosis.hpp"
#include "solid_space/geom2d/region.hpp"
#include "solid_space/robust/tolerance.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace solid_space::geom2d {

/// How a boundary detection ended.
enum class BoundaryOutcome : std::uint8_t {
    /// The picked point is enclosed: loops holds the contour and the islands.
    enclosed,
    /// The picked point lies on a curve (within the tolerance, step 4):
    /// there is no region to fill. nearCurve says which curve.
    onCurve,
    /// No closed contour encloses the picked point: the region is open, or
    /// the point is outside every closed contour. openEnds says where the
    /// curves around it are open.
    notEnclosed,
};

/// A free end of an input curve: an end that meets no other curve.
struct OpenEnd {
    /// Index of the curve in the span given to detectBoundary.
    std::size_t curve = 0;
    /// True for the start of the curve, false for its end.
    bool atStart = true;
    /// The end: the doubles of the data for a Segment or a PolylineSegment,
    /// the point of endpoints() (curves.hpp) for an Arc.
    Point2 point;
};

/// The result of detectBoundary.
struct DetectedBoundary {
    BoundaryOutcome outcome = BoundaryOutcome::notEnclosed;
    /// With enclosed: the contour first (depth 0), then the islands, by
    /// increasing depth; at equal depth by their vertex sequences, each from
    /// its first vertex, compared in exact lexicographic order of (x, y),
    /// a shorter sequence first when one is the start of the other. For this
    /// order the sequence of a Circle is its single point center - (radius,
    /// 0). Empty otherwise.
    std::vector<RegionLoop> loops;
    /// With onCurve: the lowest index of the curves within the tolerance of
    /// the picked point (for a link, the lower index of its two curves).
    std::size_t nearCurve = 0;
    /// With notEnclosed: the free ends left after steps 1-3 of the first
    /// connected group of curves met by the half-line from the picked point
    /// toward +x that has free ends (step 5), by curve index, the start
    /// before the end. Empty when there is none, and with the other outcomes.
    std::vector<OpenEnd> openEnds;
    /// The curves left out (step 1), by index, with any outcome.
    std::vector<SkippedInput> skipped;
};

/// The smallest closed contour around a picked point, and its islands, among
/// the given curves. A polyline is given as its pieces (curves.hpp, pieces).
/// Inputs are numbered as in intersectAlong: the picked point is input 0,
/// curve i is input i + 1.
///
/// The exact result is defined by these steps. For two curves, epsilon is
/// policy.epsilon(M) with M the magnitude of the two (curves.hpp). In
/// steps 2 and 3, use the magnitude of the input Arc for a converted Arc.
/// For the picked point and a curve, M also counts the coordinates of the
/// point.
/// 1. Data. A curve with invalidValue, outOfDomain or degenerateCurve
///    (curves.hpp, "Checking the data"), or with a point of the curve (for
///    an Arc or a curved PolylineSegment, of its whole circle) with a
///    coordinate beyond robust::kMaxCoordinate (outOfDomain), is left out
///    and listed in skipped. An Arc of a whole circle becomes its Circle.
///    Any other Arc becomes the PolylineSegment from its rounded start to
///    its rounded end (endpoints()) with a rounded bulge, or two of them,
///    split at a rounded middle point, when its included angle exceeds half
///    a turn; each within kRelativeLoopError / 2 of the arc (region.hpp,
///    with the same M and S). One
///    whose rounded ends coincide is dropped without a diagnosis. The steps
///    below are exact on these doubles.
///    A free end is an end of a Segment or a PolylineSegment (not of a
///    Circle) that is not a common point of its curve with another curve.
/// 2. Links. For every free end e and every other end f, of another curve or
///    the other end of the same curved PolylineSegment, such that e and f are
///    distinct points at an exact distance of at most epsilon for their
///    curves, the Segment from e to f is added: a link, once for each pair.
/// 3. Extensions. From a free end e without a link, follow the extension
///    outward: the ray beyond e for a straight piece, or the rest of the
///    circle in the direction of the piece beyond its end or against it
///    beyond its start.
///    Take the first point P on that extension common to another curve of
///    step 1, including exact tangencies. If the exact distance from e to
///    P is at most epsilon for the curve of e and a curve met at P, extend
///    the original piece to P. Otherwise e stays free. All ends use the
///    curves of step 1 before any extensions. When several curves meet at
///    P, step 5 governs the resulting pieces.
/// 4. Picked point. If the exact distance from the picked point to a curve,
///    as extended in step 3, or to a link is at most epsilon, the outcome is
///    onCurve.
/// 5. Arrangement. The curves and links of steps 1-3 are split at every
///    point where they meet each other or end; a stretch shared by several
///    of them is a single piece, credited to the curve of lowest index (to a
///    curve before a link). The picked point lies in one face F.
///    If F is unbounded the outcome is notEnclosed. openEnds then comes from
///    the connected groups of curves and links (two are connected when they
///    share a point), taken in the order in which the half-line from the
///    picked point toward +x first meets them (exactly): the first one that
///    has free ends gives them. Free ends here are those of step 1 that
///    steps 2 and 3 left free.
/// 6. Loops. The boundary of a face is run with the face on the left,
///    leaving out the pieces that have the face on both sides, and is split
///    into simple loops at every vertex it passes more than once. The
///    contour is the loop of F that encloses F. The islands are the other
///    loops of F, and, for every connected group lying inside the contour
///    other than the group of the contour, the loops of the boundary of the
///    unbounded face of that group taken alone. A piece inside a group, with
///    a face of the group on both sides, is never part of an island.
/// 7. Depth. The depth of a loop is the number of the other loops of steps 6
///    (the contour included) that enclose it: every point of it lies inside
///    or on that loop, and some point lies inside.
///
/// The returned loops (region.hpp, RegionLoop) all run counterclockwise:
/// - a loop made of a whole input Circle or whole-circle Arc is that
///   Circle, with its doubles, if step 5 credits every piece of the loop
///   to that same input curve;
/// - otherwise a closed Polyline that starts at its exactly smallest vertex
///   in lexicographic order of (x, y). Each segment is a piece of step 5 or
///   part of one: a curved stretch with an included angle above half a turn
///   is returned as two segments of equal angle. Straight segments have
///   bulge 0. An end of the data of step 1 that is a vertex is returned with
///   its doubles; every other vertex (where curves meet, where an end was
///   moved, the middle of a split stretch) is within kRelativePointError
///   (curves.hpp) of the exact one. Each segment is within kRelativeLoopError
///   of the exact stretch of the given curve or link. As endpoints() does,
///   a vertex near robust::kMaxCoordinate stays within its error even
///   beyond it.
///
/// Bands (ADR 0013, point 4). Steps 2-7 are exact on the data of step 1,
/// which stands for each Arc within kRelativeLoopError / 2. Where an outcome,
/// the pieces, their number or order, the loops or their depths would change
/// if an Arc moved by up to that error (an arc that touches or nearly
/// touches another curve or the picked point), they are free. Elsewhere they
/// are determined.
///
/// Limits (ADR 0014): a gap wider than the tolerance is not closed; an end
/// whose first meeting on its extension is beyond the tolerance stays
/// free, even if it lies close to a curve (for example, at a near tangency).
/// For a group other than the contour's group, an island that touches
/// another curve of its group from inside counts only through the group's
/// outer boundary. The result may differ from AutoCAD in these cases.
///
/// Diagnosis invalidValue when a coordinate of the picked point is not
/// finite, outOfDomain when its magnitude is beyond robust::kMaxCoordinate
/// (input 0). A bad curve does not fail the call: it is listed in skipped,
/// with input i + 1.
[[nodiscard]] Result<DetectedBoundary> detectBoundary(Point2 picked, std::span<const Curve> curves,
                                                      const robust::TolerancePolicy& policy);

/// How two chosen contours are placed when neither encloses the other in a
/// way that gives a depth.
enum class ContourConflictKind : std::uint8_t {
    /// One has points inside and points outside the other, or each encloses
    /// the other.
    crossing,
    /// They are the same set of points.
    same,
};

/// Two chosen contours whose relation gives no depth.
struct ContourConflict {
    /// Indices in the span given to nestContours, first < second.
    std::size_t first = 0;
    std::size_t second = 0;
    ContourConflictKind kind = ContourConflictKind::crossing;
};

/// The result of nestContours.
struct ContourNesting {
    /// One entry per contour: how many of the other contours enclose it;
    /// nullopt for a contour left out.
    std::vector<std::optional<std::size_t>> depth;
    /// The pairs in conflict, by first and then by second.
    std::vector<ContourConflict> conflicts;
    /// The contours left out, by index, with input as their index.
    std::vector<SkippedInput> skipped;
};

/// Which chosen contours lie inside which: the islands of a hatch whose
/// boundaries are selected (Select objects). Exact, with no tolerance and no
/// construction.
///
/// Each contour is a Loop (region.hpp). Its inside is the set of points that
/// it encloses an odd number of times and that are not on it, so a
/// self-intersecting polyline is accepted. A contour is left out and listed
/// in skipped with invalidValue or outOfDomain as in curves.hpp, "Checking
/// the data" (also when a point of a piece, or of the whole circle of a
/// curved piece, has a coordinate beyond robust::kMaxCoordinate), or with
/// degenerateCurve when it is exactly a point: a Circle of radius zero, a
/// polyline with no piece left after dropping the pieces that are exactly a
/// point.
///
/// For two contours A and B kept:
/// - A encloses B when some point of B is inside A and none is outside A
///   (B may touch A or run along it);
/// - they are in conflict, listed once, when they are the same set of
///   points (same), or one has points both inside and outside the other, or
///   each encloses the other (crossing);
/// - otherwise neither encloses the other (apart, or touching from outside).
/// The depth of a contour is the number of contours that enclose it. A
/// conflict adds nothing to the depth of either contour.
[[nodiscard]] ContourNesting nestContours(std::span<const Loop> contours);

}  // namespace solid_space::geom2d
