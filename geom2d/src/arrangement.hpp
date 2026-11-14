// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// The arrangement of curves of the plane (ADR 0014, "Che cosa rende la
// risposta coerente", point 1, and Decisione, points 4, 6 and 9;
// boundary.hpp, detectBoundary, steps 5-7). Internal to geom2d: nothing here
// is part of the public contract. Stage 0.3, package PS6; PS9
// (detectBoundary) uses it without changing it.
//
// What is here:
// - ArrangementCurve: a checked curve, possibly moved to exact ends on its
//   extension (step 3), with a priority for the stretches it shares;
// - Arrangement: the curves split at every point where they meet or end, a
//   stretch shared by several curves kept once and credited to the curve of
//   lowest priority, the pieces sorted around every vertex (local_order.hpp),
//   the boundary cycles of the faces, the connected groups and the boundary
//   of the unbounded face of every group taken alone;
// - the face of a point, the simple loops of a cycle (step 6), the direction
//   of a loop, and whether a point or a loop lies inside a loop (step 7);
// - Box and overlappingPairs(): conservative bounding boxes, so that the pairs
//   of curves far apart are never computed (ADR 0014, point 9). PS9 may use
//   them for its own steps 1-4.
//
// Words. A piece runs along its credited curve from vertex `from` to vertex
// `to`, in the direction of that curve. Every piece has two darts: dart 2p
// runs from `from` to `to`, dart 2p + 1 back. A dart has the face on its left;
// next() is the dart that follows it along the boundary of that face: at the
// head of the dart, the dart leaving it just before the reversed dart in the
// counterclockwise order. A cycle is a closed sequence of darts linked by
// next(). In a group (the curves connected by common points), every bounded
// face of the group taken alone has one cycle, which runs counterclockwise
// around it, and the unbounded face of the group has one cycle, its hole
// cycle, which runs clockwise around the group. In the whole arrangement a
// bounded face is the region inside a non-hole cycle minus the groups inside
// it, and the hole cycles of those groups are the other parts of its
// boundary.
//
// Exactness. Every decision is exact, with no tolerance, on the doubles of the
// curves and on the exact ends given: the meetings and positions of stage 0.2
// (carriers.hpp, positions.hpp), the order around a point (local_order.hpp)
// and a parity count written here. The bounding boxes are rounded outward
// with a generous margin (arrangement.cpp, inflated()), so they only ever let
// through pairs that are not needed, never drop one: the result does not
// depend on them. No floating-point filter enters a decision.
//
// Why a parity count of its own and not winding.hpp. The vertices of the
// arrangement are exact points with radicals (meetings of lines and circles,
// moved ends), so the pieces are not curves of doubles, as LoopPieces of
// winding.hpp needs. The count below uses the same method (the half-line
// y = Py + e toward +x, parts monotone in y), on pieces between exact points.
//
// Determinism (ADR 0008, point 5): all the computation is in arrangement.cpp;
// every order is exact, with ties broken by indices of the input, never by
// addresses or unordered containers. The functions may allocate, so they are
// not noexcept; std::bad_alloc is the only exception that may leave them
// (ADR 0003, point 5).

#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "local_order.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace solid_space::geom2d::detail {

/// A bounding box in doubles, rounded outward: it contains every point it
/// stands for. It may be infinite in some directions (a circle whose centre
/// overflows the doubles), never NaN.
struct Box {
    double minX = 0.0;
    double minY = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
};

/// True when the two boxes share a point (closed boxes).
[[nodiscard]] bool overlaps(const Box& a, const Box& b) noexcept;

/// The pairs (i, j), i < j, of boxes that overlap, sorted by i and then by j.
/// Every pair of curves that share a point is among them when the boxes
/// contain the curves. Sweeps the boxes sorted by minX: about n log n plus
/// the pairs whose x ranges overlap, not all n^2 pairs.
[[nodiscard]] std::vector<std::pair<std::size_t, std::size_t>> overlappingPairs(
    std::span<const Box> boxes);

/// A curve given to the arrangement.
struct ArrangementCurve {
    /// The curve: a Segment, a Circle, a PolylineSegment or an Arc, already
    /// checked and not degenerate. A Circle or a whole-circle Arc is a whole
    /// circle; any other curve is a stretch from its start to its end.
    CurveMeaning meaning;
    /// Where the stretch starts, when not at the start of the curve: an exact
    /// point on its carrier, before the start along the extension (step 3).
    /// For a circular curve the stretch runs from there in the direction of
    /// the curve. Nothing for a whole circle.
    std::optional<ExactPoint> start;
    /// Where the stretch ends, when not at the end of the curve: an exact
    /// point on its carrier, past the end along the extension. Nothing for a
    /// whole circle.
    std::optional<ExactPoint> end;
    /// A stretch shared by several curves is credited to the curve of lowest
    /// priority, then of lowest index in the span. For detectBoundary, step
    /// 5: the index of the curve for a curve, larger numbers for the links.
    std::size_t priority = 0;
};

/// The box of a curve with its moved ends: the whole circle for a circular
/// curve, the stretch for a straight one.
[[nodiscard]] Box boxOf(const ArrangementCurve& curve);

/// A piece of the arrangement: a stretch of one curve between two vertices,
/// with no vertex inside, in the direction of that curve. from == to only for
/// a whole circle with a single vertex on it.
struct ArrangementPiece {
    /// The credited curve: the index in the span given to Arrangement.
    std::size_t curve = 0;
    std::size_t from = 0;
    std::size_t to = 0;
};

/// The arrangement of some curves. Preconditions (asserted in debug where
/// cheap): every curve is checked and not degenerate; a moved end lies on the
/// carrier of its curve; a stretch is simple: a straight one has start before
/// end along its line, a circular one turns by more than zero and less than a
/// whole turn from its start to its end.
class Arrangement {
public:
    /// Splits the curves, merges the shared stretches and traces the faces.
    /// The curves are copied: the span may go away afterwards.
    explicit Arrangement(std::span<const ArrangementCurve> curves);

    // --- Vertices and pieces -------------------------------------------

    /// The number of vertices: the distinct exact points where curves meet
    /// or end, plus one point on each whole circle that meets nothing (its
    /// start, at angle zero). Vertices are numbered by increasing (x, y),
    /// exactly.
    [[nodiscard]] std::size_t vertexCount() const noexcept { return m_vertices.size(); }

    /// The exact point of a vertex. All the pieces through it use this same
    /// representation (local_order.hpp, LocalOrder).
    [[nodiscard]] const ExactPoint& vertex(std::size_t v) const;

    /// The pieces, by curve and then along the curve from its start.
    [[nodiscard]] std::span<const ArrangementPiece> pieces() const noexcept { return m_pieces; }

    /// The number of darts: twice the number of pieces.
    [[nodiscard]] std::size_t dartCount() const noexcept { return 2 * m_pieces.size(); }

    /// The piece of a dart, its twin (the same piece the other way) and
    /// whether it runs against the direction of its curve.
    [[nodiscard]] static std::size_t pieceOf(std::size_t dart) noexcept { return dart / 2; }

    [[nodiscard]] static std::size_t twin(std::size_t dart) noexcept { return dart ^ 1U; }

    [[nodiscard]] static bool reversed(std::size_t dart) noexcept { return (dart & 1U) != 0; }

    /// The vertex a dart leaves and the vertex it reaches.
    [[nodiscard]] std::size_t tail(std::size_t dart) const;
    [[nodiscard]] std::size_t head(std::size_t dart) const;

    /// The half piece of a dart at its tail: the carrier of its curve and its
    /// heading (local_order.hpp).
    [[nodiscard]] HalfPiece halfPiece(std::size_t dart) const;

    /// The darts leaving a vertex, counterclockwise from +x (local_order.hpp:
    /// by tangent, then by curvature). No two have the same departure: equal
    /// ones were a shared stretch, kept once.
    [[nodiscard]] std::span<const std::size_t> around(std::size_t v) const;

    /// The dart after this one along the boundary of the face on its left.
    [[nodiscard]] std::size_t next(std::size_t dart) const;

    // --- Cycles and groups ---------------------------------------------

    /// The cycles, each from its dart of lowest index, in the order of
    /// next(); numbered by their first dart.
    [[nodiscard]] std::size_t cycleCount() const noexcept { return m_cycleStart.size(); }

    [[nodiscard]] std::span<const std::size_t> cycle(std::size_t c) const;
    [[nodiscard]] std::size_t cycleOf(std::size_t dart) const;

    /// True for the hole cycle of a group (its unbounded face, clockwise);
    /// false for the boundary of a bounded face (counterclockwise).
    [[nodiscard]] bool isHole(std::size_t c) const;

    /// The connected groups: two curves are in the same group when they share
    /// a point, directly or through other curves. Numbered by their curve of
    /// lowest index. A curve whose pieces were all credited to others still
    /// has its group.
    [[nodiscard]] std::size_t groupCount() const noexcept { return m_holeCycle.size(); }

    [[nodiscard]] std::size_t groupOfCurve(std::size_t curve) const;
    [[nodiscard]] std::size_t groupOfCycle(std::size_t c) const;

    /// The hole cycle of a group: the boundary of its unbounded face taken
    /// alone, which runs clockwise around the group.
    [[nodiscard]] std::size_t holeCycle(std::size_t group) const;

    /// A vertex of the group: its smallest vertex in (x, y).
    [[nodiscard]] std::size_t groupVertex(std::size_t group) const;

    // --- Faces, loops and parity ---------------------------------------

    /// The face that contains a point lying on no piece: the cycle that bounds
    /// it from outside (a non-hole cycle), or nothing for the unbounded face.
    /// The point may have a radical. Exact.
    [[nodiscard]] std::optional<std::size_t> faceOf(const ExactPoint& point) const;

    /// The boundary of a cycle as step 6 of detectBoundary runs it: the darts
    /// whose twin lies in the same cycle (the piece has the face on both
    /// sides) are left out, and the rest is split into simple loops at every
    /// vertex it passes more than once. Leaving those pieces out may cut the
    /// cycle into several closed walks (two contours joined by a bridge): the
    /// walk goes on from the head of a dart along the first dart left met
    /// turning clockwise from it, as next() does without those pieces. Each
    /// loop is a closed sequence of darts, each starting where the previous
    /// one ends, with no vertex twice. The walks are taken by their first
    /// dart in the cycle, and the loops of a walk in the order in which they
    /// close. Empty when every dart is left out.
    [[nodiscard]] std::vector<std::vector<std::size_t>> simpleLoops(std::size_t c) const;

    /// True when a simple loop runs counterclockwise around the region it
    /// encloses. Exact: decided at its smallest point in (x, y).
    [[nodiscard]] bool counterclockwise(std::span<const std::size_t> loop) const;

    /// True when the point lies inside a closed sequence of darts (a cycle or
    /// a loop) by the even-odd rule. Precondition: the point lies on none of
    /// its darts. Exact; the point may have a radical.
    [[nodiscard]] bool inside(const ExactPoint& point, std::span<const std::size_t> darts) const;

    /// True when the simple loop `outer` encloses the simple loop `inner`:
    /// every point of inner lies inside or on outer, and some point inside
    /// (boundary.hpp, step 7). Preconditions: the two loops have no piece in
    /// common and do not cross (they may touch at vertices), as the loops of
    /// step 6 do. Exact.
    [[nodiscard]] bool encloses(std::span<const std::size_t> outer,
                                std::span<const std::size_t> inner) const;

private:
    // A curve as the arrangement reads it.
    struct CurveData {
        CurveMeaning meaning;
        Carrier carrier;
        Heading heading = Heading::forward;
        bool circular = false;
        bool whole = false;
        ExactPoint start;
        ExactPoint end;
        std::size_t priority = 0;
        Box box;
    };

    // A vertex: its order of the darts that leave it.
    struct VertexData {
        LocalOrder order;
        std::vector<std::size_t> darts;
    };

    // The smallest point in (x, y) of some darts: a vertex, or the leftmost
    // point of the circle of a piece, strictly inside the piece.
    struct Extreme {
        ExactPoint point;
        bool atVertex = true;
        std::size_t index = 0;
    };

    [[nodiscard]] static Sign compareOn(const CurveData& curve, const ExactPoint& a,
                                        const ExactPoint& b);
    [[nodiscard]] bool strictlyInside(std::size_t piece, const ExactPoint& point) const;
    [[nodiscard]] Extreme extremeOf(std::span<const std::size_t> darts) const;
    [[nodiscard]] std::size_t westLeft(const ExactPoint& point,
                                       std::span<const std::size_t> darts) const;
    [[nodiscard]] Box pieceBox(std::size_t piece) const;
    void split(std::span<const ArrangementCurve> curves);
    void link();
    void buildCycles();
    void buildGroups();

    std::vector<CurveData> m_curves;
    std::vector<ExactPoint> m_vertices;
    std::vector<VertexData> m_vertexData;
    std::vector<ArrangementPiece> m_pieces;
    // Position of each dart in around(tail(dart)).
    std::vector<std::size_t> m_aroundIndex;
    std::vector<std::size_t> m_cycleDarts;
    std::vector<std::size_t> m_cycleStart;
    std::vector<std::size_t> m_cycleOfDart;
    std::vector<Box> m_cycleBox;
    std::vector<std::size_t> m_groupOfCurve;
    std::vector<std::size_t> m_groupOfCycle;
    std::vector<std::size_t> m_holeCycle;
    std::vector<std::size_t> m_groupVertex;
};

}  // namespace solid_space::geom2d::detail
