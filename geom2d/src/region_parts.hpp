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
};

/// Splits and classifies a nondegenerate, checked host against checked loops.
/// For nestContours, pass Extension::none and the pieces of a single loop as
/// the region. No rounded coordinates or stations enter the classification.
[[nodiscard]] RegionParts regionParts(const CurveMeaning& host, Extension extension,
                                      std::span<const LoopPieces> loops);

}  // namespace solid_space::geom2d::detail
