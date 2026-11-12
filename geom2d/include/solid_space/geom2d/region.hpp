// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Contract with Tecnigrafo (ADR 0007, point 15): see ADR 0014, request #214.
//
// Closed loops of the plane and the regions they bound. A region is given by
// a list of loops and the even-odd rule: a point is inside the region when
// it is enclosed by an odd number of its loops and lies on none of them.
// boundary.hpp finds such loops among the curves of a drawing; other
// operations on regions use the same types.

#include "solid_space/geom2d/curves.hpp"
#include "solid_space/geom2d/diagnosis.hpp"

#include <cstddef>
#include <cstdint>
#include <variant>
#include <vector>

namespace solid_space::geom2d {

/// Largest geometric error of a piece of a returned loop. Each piece is
/// within kRelativeLoopError * max(M, S) of the exact piece it stands for,
/// and the exact piece is within the same distance of it, where S is the
/// largest absolute coordinate over all points of the exact piece and M the
/// magnitude of the curves that give pieces to the loop (curves.hpp). The
/// bound holds for M at least 2^-50, as every construction bound of geom2d
/// (curves.hpp). 2^-44 is about 5.7e-14, 4 times below the relative
/// tolerance of ADR 0009.
inline constexpr double kRelativeLoopError = 0x1p-44;

/// A closed loop: a whole circle, or a polyline taken as closed. The last
/// segment of the polyline runs from its last vertex back to the first, with
/// the bulge of the last vertex, whatever its `closed` flag says. A Circle
/// runs counterclockwise (curves.hpp).
using Loop = std::variant<Circle, Polyline>;

/// What a piece of a returned loop is made of.
enum class LoopPieceSource : std::uint8_t {
    /// A stretch of an input curve.
    curve,
    /// A link: the straight segment between distinct ends joined within
    /// the tolerance of their curves (boundary.hpp, detectBoundary, step 2).
    link,
};

/// Where a piece of a returned loop comes from, so that the caller can
/// relate the loop to its own entities.
struct LoopPieceOrigin {
    LoopPieceSource source = LoopPieceSource::curve;
    /// curve: the index of the input curve. link: the lower of the indices
    /// of the two curves whose ends the link joins.
    std::size_t curve = 0;
    /// curve: equal to curve. link: the higher of the two indices (equal to
    /// curve when the link joins the two ends of the same curve).
    std::size_t otherCurve = 0;
    /// curve: true when the piece runs against the direction of its curve.
    /// link: always false.
    bool reversed = false;
};

/// A loop found among the curves of a drawing, with the origin of its pieces.
struct RegionLoop {
    /// The loop, running counterclockwise. A Polyline has `closed` set.
    Loop loop;
    /// One entry per piece, in order: for a Polyline, entry i is the segment
    /// that starts at vertex i; for a Circle, a single entry.
    std::vector<LoopPieceOrigin> origins;
    /// How many other loops of the same result enclose this one: 0 for the
    /// outer boundary, 1 for an island in it, 2 for an island in an island,
    /// and so on.
    std::size_t depth = 0;
};

/// An input that an operation on many inputs could not use: it is left out
/// and listed, and the call goes on with the others.
struct SkippedInput {
    /// Index of the input in the span given to the operation.
    std::size_t index = 0;
    /// Why: its code, with input as the operation numbers its inputs.
    Diagnosis diagnosis;
};

}  // namespace solid_space::geom2d
