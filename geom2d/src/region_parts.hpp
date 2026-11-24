// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "solid_space/geom2d/region.hpp"
#include "winding.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace solid_space::geom2d::detail {

/// Validates a loop under the shared rules of clipToRegion and nestContours.
/// The result retains the original piece indices and drops point pieces.
[[nodiscard]] Result<LoopPieces> checkedLoopPieces(const Loop& loop, std::size_t input);

/// The classes of the open parts between cuts, viewed along the host.
enum class PartClass : std::uint8_t {
    outside,
    inside,
    boundaryLeft,
    boundaryRight,
};

struct RegionParts {
    /// Distinct exact cuts in host order, including the ends of a bounded
    /// host. A closed host lists its start twice: first at the start and again
    /// at the end of the turn.
    std::vector<ExactPoint> cuts;
    /// One class between each consecutive pair of cuts. For an extended line
    /// the two unbounded tails (or the whole line if there are no cuts) are
    /// implicitly outside, since all loop pieces are bounded.
    std::vector<PartClass> classes;
    bool closed = false;
    /// One flag per class: the open part runs along a loop piece,
    /// independently of the parity of its two sides.
    std::vector<bool> onLoop;
};

/// The loops of a region indexed for regionParts() (package FS1 of the 0.3
/// performance work): the bands in y of LoopIndex (winding.hpp) and, for
/// straight hosts, the boxes of the pieces sorted along the normal of a
/// direction, so that parallel hosts (a hatch) find the pieces they may meet
/// without running through all of them (ADR 0015, point 9). Built once for
/// many hosts. It refers to the loops, which must outlive it and not change.
class RegionIndex {
public:
    explicit RegionIndex(std::span<const LoopPieces> loops,
                         PieceSearch search = PieceSearch::automatic);

    [[nodiscard]] const LoopIndex& loops() const noexcept { return m_loops; }

    /// The slots (LoopIndex), in increasing order, of the pieces that may
    /// meet the carrier of a host: every other piece is certain to share no
    /// point with it (region_parts.cpp, "Which pieces a host meets"). With
    /// everyPiece, every slot. A straight host that is not nearly parallel to
    /// the direction of the last sorting sorts the boxes along its own normal
    /// first: hence not const. Precondition: carrier is the carrier of host,
    /// which is not degenerate.
    [[nodiscard]] std::vector<std::size_t> meeting(const CurveMeaning& host,
                                                   const Carrier& carrier);

private:
    void sortAlongNormal(Point2 normal);

    LoopIndex m_loops;
    // The normal of the sorting, (0, 0) before the first one.
    Point2 m_normal{};
    IntervalIndex m_alongNormal;
};

/// Splits and classifies a nondegenerate, checked host against checked loops.
/// For nestContours, pass Extension::none and the pieces of a single loop as
/// the region. No rounded coordinates or stations enter the classification:
/// the boxes of the index only choose which pieces are looked at, and the
/// result is the same with PieceSearch::everyPiece. The overload with a span
/// builds a RegionIndex for the call. The cuts are ends of the checked host
/// or points of the loops, within the domain, so they meet the precondition
/// of the functions of winding.hpp.
[[nodiscard]] RegionParts regionParts(const CurveMeaning& host, Extension extension,
                                      RegionIndex& loops);
[[nodiscard]] RegionParts regionParts(const CurveMeaning& host, Extension extension,
                                      std::span<const LoopPieces> loops);

}  // namespace solid_space::geom2d::detail
