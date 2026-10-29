// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Draft contract (ADR 0007, point 15): see ADR 0010, request #20.
// Declarations only: nothing here may be used before the package that
// implements it is merged.
//
// What is exact and what is rounded. Every decision below is exact about the
// curves as defined in curves.hpp: how many common points there are, whether
// the curves cross, touch or share a stretch, where each point lies on each
// curve (Location), the order of the points along a curve, and which points
// of different pairs are the same point. So the answers never contradict
// each other, nor side() in side.hpp. Points and stations are rounded within
// kRelativePointError and kRelativeStationError (curves.hpp). A rounded point
// does not lie exactly on the curves: decisions about an intersection are
// read from the result, never by testing its rounded point again.

#include "solid_space/geom2d/curves.hpp"
#include "solid_space/geom2d/diagnosis.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace solid_space::geom2d {

/// Which part of a curve takes part in an operation.
enum class Extension : std::uint8_t {
    /// The curve as given.
    none,
    /// The curve and its extension: the whole line of a straight curve, the
    /// whole circle of an arc (see curves.hpp).
    extended,
};

/// Where a point lies on a curve or on its extension. Decided exactly.
enum class Location : std::uint8_t {
    /// On the extension of a straight curve, before its start.
    beforeStart,
    /// Exactly at the start.
    atStart,
    /// On the curve, strictly between start and end. Every point of a Circle
    /// is here, and so is every point of a whole-circle Arc but its start.
    inside,
    /// Exactly at the end (not used for whole circles).
    atEnd,
    /// On the extension, past the end. For an arc this is the whole rest of
    /// its circle.
    afterEnd,
};

/// How the two curves meet at a point, decided exactly on the whole lines
/// and circles that carry them.
enum class Contact : std::uint8_t {
    /// They cross: different tangent directions.
    crossing,
    /// They touch: the same tangent line, different curves (a line tangent to
    /// a circle, two tangent circles).
    touching,
    /// They lie on the same line or the same circle and meet at this point
    /// only: the two curves follow on from each other, without overlapping.
    sameCurve,
};

/// A position on a curve: where, and how far along it.
struct CurvePosition {
    Location location = Location::inside;
    /// Arc length from the start of the curve, in the direction of the curve
    /// (rounded, see kRelativeStationError). Negative before the start of a
    /// straight curve; between the length of an arc and the length of its
    /// whole circle after its end. For a Circle it is measured
    /// counterclockwise from its point at angle zero.
    double station = 0.0;
};

/// A point where two curves meet.
struct IntersectionPoint {
    /// Rounded within kRelativePointError.
    Point2 point;
    Contact contact = Contact::crossing;
    CurvePosition onFirst;
    CurvePosition onSecond;
};

/// One end of a stretch shared by two curves.
struct OverlapEnd {
    /// Rounded within kRelativePointError.
    Point2 point;
    CurvePosition onFirst;
    CurvePosition onSecond;
};

/// A stretch shared by two curves on the same line or the same circle.
struct Overlap {
    /// True when the shared stretch is the whole carrying line or circle (two
    /// extended collinear segments, two equal circles); from and to are then
    /// the start of the first curve.
    bool whole = false;
    /// The ends of the stretch, in the direction of the first curve.
    OverlapEnd from;
    OverlapEnd to;
};

/// All the common points of two curves.
struct PairIntersections {
    /// The isolated common points, in the exact order along the first curve
    /// (by station; for a closed curve from its start). No two of them are
    /// the same exact point. Their stations never decrease along the list.
    std::vector<IntersectionPoint> points;
    /// The shared stretches, in the order of their start along the first
    /// curve. Their ends are not repeated in points.
    std::vector<Overlap> overlaps;
    /// Common points whose exact coordinates have magnitude beyond
    /// robust::kMaxCoordinate (two extended, almost parallel lines): they are
    /// not listed, only counted.
    std::size_t omittedBeyondDomain = 0;
};

/// The common points of two curves, each taken as given or extended.
///
/// Symmetric: intersect(b, eb, a, ea) gives the same points, with the same
/// bits, with onFirst and onSecond swapped and sorted along b.
///
/// Diagnosis invalidValue, outOfDomain or degenerateCurve, about input 0 or 1
/// (see curves.hpp, "Checking the data").
[[nodiscard]] Result<PairIntersections> intersect(const Curve& first, Extension firstExtension,
                                                  const Curve& second, Extension secondExtension);

/// A cutter that meets the host at a point.
struct CutterContact {
    /// Index of the cutter in the span given to intersectAlong.
    std::size_t cutter = 0;
    Contact contact = Contact::crossing;
    CurvePosition onCutter;
};

/// A point of the host where one or more cutters meet it.
struct Cut {
    /// Rounded within kRelativePointError.
    Point2 point;
    CurvePosition onHost;
    /// Every cutter through this exact point, by increasing index.
    std::vector<CutterContact> cutters;
};

/// A stretch that a cutter shares with the host.
struct CutterOverlap {
    std::size_t cutter = 0;
    /// The stretch, with "first" being the host and "second" the cutter.
    Overlap overlap;
};

/// A cutter that intersectAlong could not use.
struct SkippedCutter {
    std::size_t cutter = 0;
    /// Why: invalidValue, outOfDomain or degenerateCurve, with input i + 1.
    Diagnosis diagnosis;
};

/// Everything that a set of cutters does to one host curve.
struct HostIntersections {
    /// The points of the host met by at least one cutter, in the exact order
    /// along the host. Points of different cutters that are the same exact
    /// point form a single Cut. Stations never decrease along the list.
    std::vector<Cut> cuts;
    /// The stretches shared with cutters, by start along the host, then by
    /// cutter index.
    std::vector<CutterOverlap> overlaps;
    /// Cutters with invalid or degenerate data, left out and listed here.
    std::vector<SkippedCutter> skipped;
    /// As in PairIntersections, over all cutters.
    std::size_t omittedBeyondDomain = 0;
};

/// The intersections of one host curve with many cutters, as TRIM and EXTEND
/// need them: one exact order along the host, and exactly coincident points
/// of different cutters merged. Each cutter alone gives the same points as
/// intersect(host, hostExtension, cutter, cutterExtension).
///
/// Diagnosis invalidValue, outOfDomain or degenerateCurve about the host
/// (input 0). A bad cutter does not fail the call: it is listed in skipped.
[[nodiscard]] Result<HostIntersections> intersectAlong(const Curve& host, Extension hostExtension,
                                                       std::span<const Curve> cutters,
                                                       Extension cutterExtension);

}  // namespace solid_space::geom2d
