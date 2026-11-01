// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// The exact steps of offsetPolyline (offset.hpp; ADR 0012, point 4): the
// points of an offset polyline, the joint of two offset pieces, and the course
// of a piece between its new ends. Internal to geom2d: nothing here is part of
// the public contract. Stage 0.2, package P14; offset_polyline.cpp drives the
// steps and checks the result.
//
// One tower per decision. The joints of an offset polyline are meetings of
// offset carriers (offset_carriers.hpp), whose numbers carry the root of the
// length of every slanted segment and of the radius of every curved piece: a
// polyline of many pieces needs far more roots than a tower holds
// (exact/tower.hpp, kMaxTowerRoots). So an exact point is never kept as a
// number: it is kept as a PointRecipe, which says how to build it again, and
// every decision builds the points it needs in a fresh OffsetScope, with only
// the roots of the pieces it involves. The largest decisions are:
// - the course of a piece P whose two ends are joints: the roots of P, of the
//   pieces before and after it, and the discriminants of the two joints, five
//   in all (exact/tower.hpp);
// - whether the ends of A and B differ after pieces between them were removed:
//   the roots of A and B, of the removed pieces next to them, and the
//   discriminants of their joints with A and B, at most six: the limit of the
//   tower.
// The tower refuses a seventh root and says so (towerFull): the caller turns it
// into a diagnosis, never into a silent fallback.
//
// Decisions are exact and use no tolerance: the tolerance rule of ADR 0009
// enters only the final check of offset_polyline.cpp. All the computation is
// in offset_joints.cpp (ADR 0008, point 5); no trigonometry. The functions may
// allocate (exact numbers), so they are not noexcept.

#include "curve_meaning.hpp"
#include "exact/tower.hpp"
#include "offset_carriers.hpp"
#include "solid_space/geom2d/curves.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

namespace solid_space::geom2d::detail {

/// The pieces of the input that are not single points, as offsetPolyline keeps
/// them after its first drop, and the distance. Each meaning is the meaning of
/// a PolylineSegment that is not degenerate.
struct OffsetSource {
    std::vector<CurveMeaning> pieces;
    double distance = 0.0;
};

/// One end of a piece.
enum class PieceEnd : std::uint8_t {
    start,
    end,
};

/// How to build an exact point of the offset again in any tower.
struct PointRecipe {
    enum class Kind : std::uint8_t {
        /// An end of a piece moved by the distance, as offsetCurve moves it
        /// (step 1 of offsetPolyline).
        moved,
        /// A meeting of the offset carriers of two pieces: point `index` of
        /// meetOffset(carrier(piece), carrier(other)) (step 2).
        meeting,
    };

    Kind kind = Kind::moved;
    /// moved: the piece (an index of OffsetSource::pieces); meeting: the
    /// piece A of the joint.
    std::size_t piece = 0;
    /// meeting: the piece B of the joint.
    std::size_t other = 0;
    /// moved: which end.
    PieceEnd end = PieceEnd::start;
    /// meeting: which point of the meeting.
    std::size_t index = 0;

    [[nodiscard]] static PointRecipe movedEnd(std::size_t piece, PieceEnd end) noexcept {
        PointRecipe recipe;
        recipe.piece = piece;
        recipe.end = end;
        return recipe;
    }

    [[nodiscard]] static PointRecipe meetingPoint(std::size_t piece, std::size_t other,
                                                  std::size_t index) noexcept {
        PointRecipe recipe;
        recipe.kind = Kind::meeting;
        recipe.piece = piece;
        recipe.other = other;
        recipe.index = index;
        return recipe;
    }
};

/// A tower and the carriers and points of one decision.
class OffsetScope {
public:
    /// Precondition: `source` outlives the scope.
    explicit OffsetScope(const OffsetSource& source) : m_source(&source) {}

    [[nodiscard]] exact::Tower& tower() noexcept { return m_tower; }

    /// The offset carrier of a piece. Nothing when its root does not fit in the
    /// tower. Precondition: the piece does not vanish at the distance.
    [[nodiscard]] std::optional<OffsetCarrier> carrier(std::size_t piece);

    /// An end of a piece moved by the distance, exactly: along the normal of a
    /// straight piece, along the radius of a curved one. It lies on carrier().
    /// Nothing when the root does not fit in the tower.
    [[nodiscard]] std::optional<TowerPoint> moved(std::size_t piece, PieceEnd end);

    /// meetOffset() of the carriers of two pieces. Nothing when a root does not
    /// fit in the tower.
    [[nodiscard]] std::optional<OffsetMeeting> meeting(std::size_t first, std::size_t second);

    /// The point of a recipe. Nothing when a root does not fit in the tower.
    [[nodiscard]] std::optional<TowerPoint> point(const PointRecipe& recipe);

private:
    const OffsetSource* m_source;
    exact::Tower m_tower;
    std::vector<std::pair<std::size_t, OffsetCarrier>> m_carriers;
    std::vector<std::pair<std::pair<std::size_t, std::size_t>, OffsetMeeting>> m_meetings;
};

/// How an exact step ended.
enum class StepStatus : std::uint8_t {
    /// The step has an answer.
    done,
    /// No joint: the carriers do not meet and no arc ends on both pieces.
    openJoint,
    /// A point would lie beyond robust::kMaxCoordinate.
    beyondDomain,
    /// An arc would turn by exactly half a turn to reach its joint, or by a
    /// full turn or more between its ends.
    selfIntersection,
    /// A decision needs more roots than a tower holds.
    towerFull,
};

/// How two pieces left next to each other are joined.
enum class JointKind : std::uint8_t {
    /// Their ends are the same point: no joint.
    none,
    /// At a common point of their carriers: A ends there, B starts there.
    meeting,
    /// By an arc of radius |distance| centred on the vertex, from the end of A
    /// to the start of B.
    arc,
};

/// Two pieces A and B left next to each other, and their current ends.
struct JointRequest {
    std::size_t first = 0;
    PointRecipe firstEnd;
    std::size_t second = 0;
    PointRecipe secondStart;
    /// The vertex V of the input at the joint.
    Point2 vertex;
    /// True when the source pieces of A and B are consecutive in the input, or
    /// made so by pieces that are single points.
    bool consecutive = false;
};

/// The joint of two pieces.
struct Joint {
    StepStatus status = StepStatus::done;
    JointKind kind = JointKind::none;
    /// meeting: the joint point, the new end of A and the new start of B.
    PointRecipe point;
    /// arc: the bulge of the arc of the joint, rounded.
    double bulge = 0.0;
};

/// Step 2 of offsetPolyline for one pair of pieces: no joint when their
/// current ends are the same point; otherwise the common point of their
/// carriers nearest the vertex (a tie goes to the first along A), checked
/// against the domain and, for an arc, against a rotation of exactly half a
/// turn from its moved end; otherwise, for consecutive source pieces, the arc
/// of radius |distance| centred on the vertex that turns as the input turns
/// there (by the tangents of the two source pieces; with opposite tangents,
/// clockwise for a positive distance); otherwise openJoint.
[[nodiscard]] Joint join(const OffsetSource& source, const JointRequest& request);

/// Where the new end of a piece lies with respect to its new start.
enum class Course : std::uint8_t {
    /// After it: the piece runs as its source does.
    forward,
    /// The same point: the piece is reduced to a point (step 4).
    point,
    /// Before it, or an arc whose sweep changed sign: the piece turned
    /// backwards (step 3).
    backward,
};

/// The course of a piece, and whether the step failed.
struct PieceCourse {
    StepStatus status = StepStatus::done;
    Course course = Course::forward;
};

/// Steps 3 and 4 of offsetPolyline for one piece with the given ends. For an
/// arc, each end is reached from its moved end by the shortest rotation; an
/// arc that would turn by a full turn or more, or reach an end by exactly half
/// a turn, gives selfIntersection.
[[nodiscard]] PieceCourse courseOf(const OffsetSource& source, std::size_t piece,
                                   const PointRecipe& start, const PointRecipe& end);

/// A point of the result rounded to doubles, or why there is none.
struct RoundedPoint {
    StepStatus status = StepStatus::done;
    Point2 point;
};

/// The point of a recipe rounded to doubles: a moved end as offsetCurve rounds
/// it (the same bits), a meeting within kRelativePointError
/// (offset_carriers.hpp, roundedPoint). beyondDomain when the exact point lies
/// beyond robust::kMaxCoordinate.
[[nodiscard]] RoundedPoint roundedPoint(const OffsetSource& source, const PointRecipe& recipe);

/// A bulge of the result rounded to double, or why there is none.
struct RoundedBulge {
    StepStatus status = StepStatus::done;
    double bulge = 0.0;
};

/// The bulge of a curved piece between new ends: tan(theta / 4), with theta
/// the angle from the start to the end along the offset circle, in (0, 2 pi),
/// positive for a counterclockwise piece. Precondition: the course of the
/// piece between these ends is forward.
[[nodiscard]] RoundedBulge arcBulge(const OffsetSource& source, std::size_t piece,
                                    const PointRecipe& start, const PointRecipe& end);

}  // namespace solid_space::geom2d::detail
