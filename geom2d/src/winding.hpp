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
//   carriers, as the decisions read it, with the box of every piece;
// - LoopIndex: some loops with their pieces sorted in bands of y by their
//   boxes (package FS1), and IntervalIndex, which does the sorting;
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
// The boxes (package FS1 of the 0.3 performance work). Every piece carries a
// box of doubles that contains it (pieceBox()) and its parts monotone in y,
// both computed once with the piece. A LoopIndex sorts the boxes in bands of
// y, so that a point only looks at the pieces whose box it can reach: the
// pieces through it lie in boxes that contain it, and the parts crossed by
// its half-line in boxes that the half-line meets. A box only ever skips a
// piece whose answer is certain (winding.cpp, "Why skipping a box is
// exact"): every answer is the one of the exact path of stage 0.3, which
// PieceSearch::everyPiece still runs, for the tests that compare the two.
//
// No floating-point filter yet: every sign is exact (local_order.hpp). All the
// computation is in winding.cpp (ADR 0008, point 5). The functions may
// allocate, so they are not noexcept.

#include "arrangement.hpp"
#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "local_order.hpp"
#include "solid_space/geom2d/region.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace solid_space::geom2d::detail {

/// The side of its centre on which a part of a circle lies.
enum class CircleHalf : std::uint8_t {
    right,  ///< x >= centre x
    left,   ///< x <= centre x
};

/// A part of a piece that is monotone in y, from one end to the other (see
/// "The method"). For a part of a circle, half says on which side of the
/// centre it lies; it is not read for a segment.
struct MonotonePart {
    ExactPoint from;
    ExactPoint to;
    CircleHalf half = CircleHalf::right;
};

/// A piece of a loop, with its exact meaning, its carrier, its box and its
/// parts monotone in y.
struct LoopPiece {
    /// Which piece of the loop it is: for a Polyline, the segment that starts
    /// at vertex `index` (region.hpp, RegionLoop::origins); 0 for a Circle.
    std::size_t index = 0;
    /// The piece as a curve: the Circle, or the PolylineSegment from vertex
    /// index to the next one (the last back to the first) with the bulge of
    /// vertex index. Never degenerate.
    CurveMeaning meaning;
    Carrier carrier;
    /// A box that contains every point of the piece (pieceBox()).
    Box box;
    /// The parts of the piece monotone in y, in the order of the piece.
    std::vector<MonotonePart> parts;
};

/// A box of doubles that contains every point of a piece exactly: for a
/// straight piece the box of its two ends, which are doubles of the data; for
/// a curved PolylineSegment the box of its two ends, widened to the box of
/// its circle (arrangement.hpp, boxOf()) on every side whose extreme point of
/// the circle lies strictly inside the arc; for any other circular curve the
/// box of its circle. It may be infinite on some sides, never NaN.
/// Precondition: the meaning is not degenerate and carrier is its carrier.
[[nodiscard]] Box pieceBox(const CurveMeaning& meaning, const Carrier& carrier);

/// A piece of a loop with its box and its parts, computed here once.
/// Precondition: as pieceBox().
[[nodiscard]] LoopPiece loopPiece(std::size_t index, CurveMeaning meaning, Carrier carrier);

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

/// Which pieces the functions below look at. The answers are the same with
/// all three.
enum class PieceSearch : std::uint8_t {
    /// boxes when the loops have more than kFewPieces pieces, everyPiece
    /// otherwise: with so few pieces, rounding the point and searching cost
    /// more than looking at all of them.
    automatic,
    /// Only the pieces whose box can matter, found through the bands in y of
    /// LoopIndex.
    boxes,
    /// Every piece, with no box: the exact path of stage 0.3, kept for the
    /// tests that compare it with boxes.
    everyPiece,
};

/// The most pieces for which PieceSearch::automatic looks at every piece.
inline constexpr std::size_t kFewPieces = 8;

/// Closed intervals of doubles, sorted by their low end, with the largest
/// high end of every run of them in a binary tree, so that the intervals
/// meeting a given one are found in about log n steps plus one step for each
/// of them, never by running through all of them.
class IntervalIndex {
public:
    IntervalIndex() = default;

    /// The intervals [lows[i], highs[i]]. Preconditions (asserted in debug):
    /// as many lows as highs, no NaN (infinities are allowed).
    IntervalIndex(std::span<const double> lows, std::span<const double> highs);

    /// The indices i, in increasing order, of the intervals that share a point
    /// with [low, high]: lows[i] <= high and highs[i] >= low.
    [[nodiscard]] std::vector<std::size_t> meeting(double low, double high) const;

private:
    // Sorted by the low end, ties by the index: the low end and the index of
    // every slot.
    std::vector<double> m_lows;
    std::vector<std::size_t> m_items;
    // A complete binary tree over m_leaves slots (a power of two): node 1 is
    // the root, the children of node k are 2k and 2k + 1, and the leaves are
    // m_leaves + slot. Every node holds the largest high end of its slots.
    std::vector<double> m_largestHigh;
    std::size_t m_leaves = 0;
};

/// A piece of the loops by its place: loop `loop` of the span, its
/// pieces[position].
struct PieceSlot {
    std::size_t loop = 0;
    std::size_t position = 0;
};

/// Some loops and their pieces sorted in bands of y by their boxes (see "The
/// boxes"). The pieces are numbered by slot, in the order of the loops and of
/// the pieces of each loop. It refers to the loops, which must outlive it and
/// not change.
class LoopIndex {
public:
    explicit LoopIndex(std::span<const LoopPieces> loops,
                       PieceSearch search = PieceSearch::automatic);

    [[nodiscard]] std::span<const LoopPieces> loops() const noexcept { return m_loops; }

    /// boxes or everyPiece: automatic is decided when the index is built.
    [[nodiscard]] PieceSearch search() const noexcept { return m_search; }

    /// The number of pieces of all the loops.
    [[nodiscard]] std::size_t size() const noexcept { return m_slots.size(); }

    [[nodiscard]] const PieceSlot& slot(std::size_t number) const { return m_slots.at(number); }

    [[nodiscard]] const LoopPiece& piece(std::size_t number) const;

    /// The box of all the pieces: it contains every point of the loops.
    [[nodiscard]] const Box& whole() const noexcept { return m_whole; }

    /// The slots, in increasing order, of the pieces whose box shares a point
    /// with the closed box `box`: every other piece lies outside it. With
    /// everyPiece, every slot.
    [[nodiscard]] std::vector<std::size_t> meeting(const Box& box) const;

private:
    std::span<const LoopPieces> m_loops;
    PieceSearch m_search = PieceSearch::boxes;
    std::vector<PieceSlot> m_slots;
    Box m_whole;
    IntervalIndex m_bands;
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
/// even-odd rule. Exact. Precondition, for every function below that takes a
/// point: its coordinates are below 2^1000 in magnitude, as nearestPoint()
/// (positions.hpp) needs, which is true of every point of a curve within the
/// domain. The overloads with a span build a LoopIndex for the call.
[[nodiscard]] PointStatus pointStatus(const ExactPoint& point, const LoopIndex& loops);
[[nodiscard]] PointStatus pointStatus(const ExactPoint& point, std::span<const LoopPieces> loops);

/// The parity of the sector of the point that starts at the direction +x or
/// contains it (see above): true for odd, inside. With the point off the
/// loops, whether it is inside. Exact.
[[nodiscard]] bool parityAboveRay(const ExactPoint& point, const LoopIndex& loops);
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
[[nodiscard]] SectorStates sectorStates(const ExactPoint& point, const LoopIndex& loops,
                                        std::span<const HalfPiece> references = {});
[[nodiscard]] SectorStates sectorStates(const ExactPoint& point, std::span<const LoopPieces> loops,
                                        std::span<const HalfPiece> references = {});

}  // namespace solid_space::geom2d::detail
