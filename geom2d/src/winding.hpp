// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// The parity of a point, and of the sectors around it, with respect to closed
// loops (region.hpp: the even-odd rule; ADR 0015, "Che cosa rende la risposta
// coerente", points 1 and 2). Internal to geom2d: nothing here is part of the
// public contract. Stage 0.3, package PS2; PS5 (clipToRegion), PS6, PS9 and
// PS10 use it without changing it.
//
// What is here:
// - LoopPieces: a Loop (region.hpp) described by its pieces and their
//   carriers, as the decisions read it;
// - pointStatus(): whether an exact point lies on a piece of the loops (and
//   on which), or else inside or outside the region with the even-odd rule;
// - sectorStates(): around an exact point, the half pieces of the loops that
//   leave it and, if given, reference half pieces (the host of a cut in its
//   two headings), sorted (local_order.hpp), and for every sector between
//   two consecutive groups whether it is inside or outside the region.
//
// The rule. The loops are all counted together: a point off the loops is
// inside when a half-line from it that meets the loops at no vertex and
// touches them nowhere crosses them an odd number of times in all (region.hpp,
// clipping.hpp). A piece shared by several loops is counted once for each loop
// (and a piece run twice by one loop twice): crossing a group of equal half
// pieces at a point changes the parity by the number of loop half pieces in
// the group, so two loops that run along the same stretch cancel there and
// leave it inside on both sides, or outside on both sides (ADR 0015, case 3).
// Reference half pieces change nothing.
//
// The method. The parity is read on the half-line y = Py + e, x > Px, for an
// infinitely small e > 0: a point of the data at height Py counts as below
// it. Every piece is split into parts monotone in y (a segment is one part;
// an arc is split where its circle has a horizontal tangent), and a part is
// crossed when exactly one of its ends lies strictly above Py. Where it is
// crossed, its point at height Py is compared with P exactly: for a segment,
// by the side of P with respect to it; for a part of a circle on the right
// (left) of its centre, from the sign of Cx - Px and the side of P with
// respect to the circle, since that point is Cx + sqrt(Q) (Cx - sqrt(Q)) with
// Q = R^2 - (Py - Cy)^2. A part crossed exactly at P is a part through P. All
// this is exact: comparisons of ExactPoint coordinates (carriers.hpp) and
// signs of points with respect to carriers.
//
// The sectors. A part through P is crossed near P only, so the count over the
// other parts is the parity of a point just above the half-line toward +x,
// infinitely close to P: of the sector that starts at the direction +x, or
// contains it (local_order.hpp, rayDeparture). Turning counterclockwise from
// there, every group of half pieces flips the parity by the number of loop
// half pieces in it. With P off the loops this is the parity of P.
//
// Independence. Every answer is a fact about the set of pieces: it does not
// depend on the order of the loops, on the direction of a loop or on the
// vertex a polyline starts from (clipping.hpp, "The decisions ... do not
// depend"). Only the order of the half pieces within a group, which follows
// the input, may change.
//
// No floating-point filter yet: every sign is exact (local_order.hpp). All the
// computation is in winding.cpp (ADR 0008, point 5). The functions may
// allocate, so they are not noexcept.

#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "local_order.hpp"
#include "solid_space/geom2d/region.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace solid_space::geom2d::detail {

/// A piece of a loop, with its exact meaning and its carrier.
struct LoopPiece {
    /// Which piece of the loop it is: for a Polyline, the segment that starts
    /// at vertex `index` (region.hpp, RegionLoop::origins); 0 for a Circle.
    std::size_t index = 0;
    /// The piece as a curve: the Circle, or the PolylineSegment from vertex
    /// index to the next one (the last back to the first) with the bulge of
    /// vertex index. Never degenerate.
    CurveMeaning meaning;
    Carrier carrier;
};

/// A loop described by its pieces, in the order of the loop. The pieces that
/// are exactly a point are left out.
struct LoopPieces {
    std::vector<LoopPiece> pieces;
};

/// The pieces of a loop. Preconditions: the loop has been checked as
/// clipToRegion and nestContours check their loops (clipping.hpp): valid data
/// within the domain, and at least one piece that is not exactly a point (a
/// Circle has a positive radius).
[[nodiscard]] LoopPieces loopPieces(const Loop& loop);

/// A piece of the loops given to the functions below: loop `loop` of the
/// span, its piece of LoopPiece::index `piece`.
struct PieceRef {
    std::size_t loop = 0;
    std::size_t piece = 0;

    friend bool operator==(const PieceRef&, const PieceRef&) = default;
};

/// Where a point lies with respect to the region of some loops.
enum class PointState : std::uint8_t {
    outside,
    inside,
    on,
};

/// The status of a point: on, with every piece through it, or inside or
/// outside the region, with no pieces.
struct PointStatus {
    PointState state = PointState::outside;
    /// With on: every piece the point lies on (its ends included), by loop
    /// and then by piece, in the order of the span and of each loop.
    std::vector<PieceRef> pieces;
};

/// The status of an exact point (it may have a radical: the meeting of a line
/// and a circle, the exact end of an arc) with respect to the loops, with the
/// even-odd rule. Exact.
[[nodiscard]] PointStatus pointStatus(const ExactPoint& point, std::span<const LoopPieces> loops);

/// The parity of the sector of the point that starts at the direction +x or
/// contains it (see above): true for odd, inside. With the point off the
/// loops, whether it is inside. Exact.
[[nodiscard]] bool parityAboveRay(const ExactPoint& point, std::span<const LoopPieces> loops);

/// The sectors around a point, with their states.
struct SectorStates {
    /// The half pieces leaving the point: first those of the loops (one for
    /// each piece that ends at the point, two for each piece through it), by
    /// loop and then by piece; then the references, in their order.
    std::vector<HalfPiece> halfPieces;
    /// The loop piece of each of the first owners.size() half pieces; the
    /// remaining half pieces are the references.
    std::vector<PieceRef> owners;
    /// The half pieces sorted around the point.
    LocalOrder order;
    /// One entry per sector: true when inside the region. Sector g lies
    /// between group g and group (g + 1) % order.groupCount(),
    /// counterclockwise (local_order.hpp). With no half pieces, a single
    /// entry: the state of the point.
    std::vector<bool> inside;

    /// The number of half pieces of the loops.
    [[nodiscard]] std::size_t loopHalfPieces() const noexcept { return owners.size(); }
};

/// The half pieces of the loops that leave an exact point, the given
/// references with them, their order and the state of every sector. The
/// point may lie on no loop: then the sectors only come from the references,
/// and all have the state of the point. Preconditions: every reference lies
/// on the point (as departureOf(), local_order.hpp).
[[nodiscard]] SectorStates sectorStates(const ExactPoint& point, std::span<const LoopPieces> loops,
                                        std::span<const HalfPiece> references = {});

}  // namespace solid_space::geom2d::detail
