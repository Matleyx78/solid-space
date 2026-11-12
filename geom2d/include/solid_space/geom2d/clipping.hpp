// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Contract with Tecnigrafo (ADR 0007, point 15): see ADR 0015, request #215.
//
// Which parts of curves lie inside a region of the plane, as a hatch with a
// pattern needs them: the lines of the pattern are cut on the region, and
// only the stretches inside it, outside its islands, are drawn. The region is
// a list of loops with the even-odd rule (region.hpp): the contour and the
// islands found by detectBoundary, or the chosen contours, picked by the
// caller for the island style. The pattern (angles, spacing, origins,
// dashes), the units and the drawing stay with the caller.
//
// What is exact and what is rounded. Every decision is exact about the
// curves and loops as given (curves.hpp): where a host meets the loops, which
// parts of the host lie inside the region, on its boundary or outside, on
// which side of a boundary stretch the region lies, and the number and order
// of stretches within each list. No tolerance is used. Only the ends are
// rounded, within the errors of curves.hpp; no decision is read from a
// rounded value, so no outcome lies in a band (ADR 0013, point 4).

#include "solid_space/geom2d/curves.hpp"
#include "solid_space/geom2d/intersection.hpp"
#include "solid_space/geom2d/region.hpp"
#include "solid_space/geom2d/side.hpp"

#include <span>
#include <vector>

namespace solid_space::geom2d {

/// An end of a stretch of a host curve.
struct StretchEnd {
    /// The point. An end of the host as given (the start or the end of a
    /// Segment or a PolylineSegment) is returned with the doubles of the
    /// data; the start or the end of an Arc, or the start of a Circle, is the
    /// point of endpoints() (curves.hpp). Any other end, where the host meets
    /// the loops, is within kRelativePointError * max(M, |p|) of the exact
    /// point (curves.hpp), with M the magnitude of the host and of every
    /// piece of the loops through that point. As endpoints() does, an end
    /// near robust::kMaxCoordinate stays within its error even beyond it.
    Point2 point;
    /// Where the end lies on the host, as in intersect() (intersection.hpp):
    /// the location is exact, the station is rounded within
    /// kRelativeStationError * max(M, |s|), with the same M. The station of
    /// the start of the host is exactly 0. On a closed host (a Circle, an Arc
    /// of a whole circle) the end of a stretch that reaches the start again
    /// is atEnd, with the length of the host as its station.
    CurvePosition onHost;
};

/// A stretch of a host inside the region: from and to in the direction of
/// the host, with a positive exact length.
struct HostStretch {
    StretchEnd from;
    StretchEnd to;
};

/// A stretch of a host that runs along the loops with the region on one side
/// only: the host lies on the boundary of the region there.
struct BoundaryStretch {
    StretchEnd from;
    StretchEnd to;
    /// The side of the host where the region lies, looking along the host as
    /// side() does (side.hpp): left or right, never on.
    Side regionSide = Side::left;
};

/// What the region does to one host. The two lists are independent: the
/// result gives neither their relative order nor the identity of ends they
/// share. The caller uses each list separately.
struct ClippedHost {
    /// The stretches inside the region, in the exact order along the host.
    std::vector<HostStretch> inside;
    /// The stretches on the boundary of the region, in the exact order along
    /// the host.
    std::vector<BoundaryStretch> boundary;
};

/// The result of clipToRegion.
struct RegionClipping {
    /// One entry per host, in the order of the hosts; empty for a host left
    /// out.
    std::vector<ClippedHost> hosts;
    /// The hosts left out, by index, with input as their index.
    std::vector<SkippedInput> skippedHosts;
    /// The loops left out, by index, with input as their index.
    std::vector<SkippedInput> skippedLoops;
};

/// The parts of each host curve that lie inside a region: the lines of a
/// hatch pattern cut on its contour and islands. Exact, with no tolerance.
///
/// The region. Each loop is checked as in nestContours (boundary.hpp): it is
/// left out and listed in skippedLoops with invalidValue or outOfDomain as in
/// curves.hpp, "Checking the data" (also when a point of a piece, or of the
/// whole circle of a curved piece, has a coordinate beyond
/// robust::kMaxCoordinate), or with degenerateCurve when it is exactly a
/// point: a Circle of radius zero, a polyline with no piece left after
/// dropping the pieces that are exactly a point. A point that lies on no
/// kept loop is in the region when the kept loops wind around it an odd
/// number of times in all: a half-line from it that meets the loops at no
/// vertex and touches them nowhere crosses them an odd number of times. With
/// no loop kept the region is empty. Loops may touch, overlap, cross each
/// other or themselves.
///
/// The hosts. A host is any Curve. With Extension::none it is taken as
/// given; with Extension::extended a straight host (a Segment, a straight
/// PolylineSegment) is its whole line, directed from start to end, and a
/// curved host is still taken as given. A host with invalidValue,
/// outOfDomain or degenerateCurve (curves.hpp, "Checking the data") is left
/// out and listed in skippedHosts. Each host is clipped on its own: its entry
/// is the same, with the same bits, as when it is the only host.
///
/// The exact result is defined by these steps, for each host kept:
/// 1. Cuts. The host is cut at every point where it meets a kept loop (where
///    it crosses or touches a piece, or where a stretch shared with a piece
///    begins or ends), and, when it is closed, at its start. The cuts split
///    it into open parts of positive length. Each part either runs along one
///    or more pieces of the loops or contains no point of the loops.
/// 2. Classes. A part off the loops is inside when its points are in the
///    region, outside otherwise. A part along the loops is classed by the
///    points near it on each side: inside when they are in the region on
///    both sides (an edge shared by two loops that cancel), boundary when on
///    one side only, with that side, outside when on neither side.
/// 3. Stretches. An inside stretch is a longest run of consecutive inside
///    parts with the cuts between them: the host passing through a vertex of
///    the loops, or touching a piece, from inside to inside does not split
///    it. A boundary stretch is a longest run of consecutive boundary parts
///    with the same side. On a closed host no stretch runs through its start.
///    A point where the host only touches the region, with outside parts on
///    both sides, is not a stretch.
///
/// The ends are rounded (StretchEnd): two distinct ends may round to the
/// same point or station, and the stations of ends closer than their errors
/// may come out in the reverse order. The order within each list is exact,
/// as are the classes and sides. Rounded points and stations cannot be used
/// to recover exact order or shared ends across the two lists.
///
/// The decisions (the stretches, their order, locations and sides) do not
/// depend on the order of the loops, on the direction of a loop or on the
/// vertex a polyline loop starts from; the rounded values may differ within
/// their errors. A Segment with its ends swapped gives the same stretches in
/// the reverse order, with from and to swapped and the sides swapped.
///
/// Limits (ADR 0015): with no tolerance, a host that runs within a rounding
/// of a piece of the loops (a pattern line computed next to a side) is
/// inside, outside or on the boundary as its exact doubles say; loops that
/// touch in the data may cross by their rounding (boundary.hpp), and then
/// give stretches shorter than their errors.
///
/// No diagnosis: bad hosts and loops are listed and the call goes on.
[[nodiscard]] RegionClipping clipToRegion(std::span<const Curve> hosts, Extension hostExtension,
                                          std::span<const Loop> region);

}  // namespace solid_space::geom2d
