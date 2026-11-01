// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// offsetPolyline (offset.hpp; ADR 0012; stage 0.2, package P14).
//
// The four steps run on the exact values (offset_joints.hpp); this file keeps
// the pieces left after each step, decides which joints change, assembles the
// result and checks it on its own doubles before returning done. The check
// compares every pair of pieces with intersect() (ADR 0012, point 11): a
// structure that skips pairs is not needed yet.

#include "curve_meaning.hpp"
#include "exact/dyadic.hpp"
#include "offset_joints.hpp"
#include "solid_space/geom2d/intersection.hpp"
#include "solid_space/geom2d/offset.hpp"
#include "solid_space/robust/predicates.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace solid_space::geom2d {
namespace {

using detail::Course;
using detail::JointKind;
using detail::OffsetSource;
using detail::PieceEnd;
using detail::PointRecipe;
using detail::StepStatus;

// A piece of the offset while the steps run.
struct LivePiece {
    /// The index in OffsetSource::pieces.
    std::size_t slot = 0;
    /// The index in pieces(input).
    std::size_t source = 0;
    PointRecipe start;
    PointRecipe end;
    /// The joint with the next piece (in a closed polyline the last piece is
    /// followed by the first); none for the last piece of an open polyline.
    JointKind jointAfter = JointKind::none;
    /// With an arc joint, its bulge.
    double gapBulge = 0.0;
    /// The joint with the next piece must be decided (again).
    bool jointPending = true;
    /// The course must be decided (again): an end changed.
    bool coursePending = true;
    Course course = Course::forward;
};

// A piece of the result, before rounding.
struct ResultPiece {
    PointRecipe start;
    PointRecipe end;
    /// The bulge, already rounded; for a source piece whose ends were cut or
    /// extended, it is computed when the result is assembled.
    double bulge = 0.0;
    bool bulgePending = false;
    OffsetPiece origin;
    std::size_t slot = 0;
};

Diagnosis towerFull() {
    return Diagnosis{DiagnosisCode::outOfDomain, 0,
                     "the exact numbers of the offset need more roots than a tower holds"};
}

PolylineOffset failedOffset(OffsetFailure failure, bool simple) {
    PolylineOffset result;
    result.outcome = OffsetOutcome::failed;
    result.failure = failure;
    result.simple = simple;
    return result;
}

PolylineOffset vanishedOffset() {
    PolylineOffset result;
    result.outcome = OffsetOutcome::vanished;
    return result;
}

OffsetFailure failureOf(StepStatus status) {
    switch (status) {
        case StepStatus::openJoint:
            return OffsetFailure::openJoint;
        case StepStatus::beyondDomain:
            return OffsetFailure::beyondDomain;
        case StepStatus::selfIntersection:
        case StepStatus::done:
        case StepStatus::towerFull:
            break;
    }
    return OffsetFailure::selfIntersection;
}

// What a step that did not end in done returns: a diagnosis when the tower ran
// out of roots, a failed offset otherwise.
Result<PolylineOffset> stopped(StepStatus status, bool simple) {
    if (status == StepStatus::towerFull) {
        return towerFull();
    }
    return failedOffset(failureOf(status), simple);
}

// The steps of offsetPolyline on the pieces left after the first drop.
class PolylineSteps {
public:
    PolylineSteps(const Polyline& polyline, std::vector<std::size_t> sources,
                  std::vector<bool> dropped, OffsetSource source)
        : m_polyline(&polyline),
          m_sources(std::move(sources)),
          m_dropped(std::move(dropped)),
          m_source(std::move(source)) {}

    Result<PolylineOffset> run(const robust::TolerancePolicy& policy);

private:
    [[nodiscard]] bool closed() const noexcept { return m_polyline->closed; }

    [[nodiscard]] std::size_t vertexCount() const noexcept { return m_polyline->vertices.size(); }

    // The vertex at the end of an input piece.
    [[nodiscard]] std::size_t vertexAfter(std::size_t piece) const noexcept {
        return (piece + 1) % vertexCount();
    }

    // True when every input piece strictly between the two, in the direction
    // of the polyline, is a single point dropped first.
    [[nodiscard]] bool consecutive(std::size_t first, std::size_t second) const;

    // Steps 2 and 3, repeated until no piece is removed. Nothing when they
    // left pieces to assemble.
    std::optional<Result<PolylineOffset>> joinAndRemove();

    // Step 2 for the pending joints.
    StepStatus joinPending();

    // Step 4 and the result, then the check.
    Result<PolylineOffset> assemble(const robust::TolerancePolicy& policy);

    [[nodiscard]] Result<PolylineOffset> check(PolylineOffset result,
                                               const robust::TolerancePolicy& policy) const;

    const Polyline* m_polyline;
    std::vector<std::size_t> m_sources;
    std::vector<bool> m_dropped;
    OffsetSource m_source;
    std::vector<LivePiece> m_live;
    bool m_simple = true;
};

bool PolylineSteps::consecutive(std::size_t first, std::size_t second) const {
    const std::size_t count = m_dropped.size();
    for (std::size_t piece = (first + 1) % count; piece != second; piece = (piece + 1) % count) {
        if (!m_dropped[piece]) {
            return false;
        }
    }
    return true;
}

StepStatus PolylineSteps::joinPending() {
    const std::size_t count = m_live.size();
    for (std::size_t k = 0; k < count; ++k) {
        auto& first = m_live[k];
        if (!first.jointPending) {
            continue;
        }
        first.jointPending = false;
        first.jointAfter = JointKind::none;
        if (!closed() && k + 1 == count) {
            continue;
        }
        auto& second = m_live[(k + 1) % count];
        detail::JointRequest request;
        request.first = first.slot;
        request.firstEnd = first.end;
        request.second = second.slot;
        request.secondStart = second.start;
        request.vertex = m_polyline->vertices[vertexAfter(first.source)].position;
        request.consecutive = consecutive(first.source, second.source);
        const auto joint = detail::join(m_source, request);
        if (joint.status != StepStatus::done) {
            return joint.status;
        }
        first.jointAfter = joint.kind;
        if (joint.kind == JointKind::meeting) {
            first.end = joint.point;
            second.start = joint.point;
            first.coursePending = true;
            second.coursePending = true;
        } else if (joint.kind == JointKind::arc) {
            first.gapBulge = joint.bulge;
        }
    }
    return StepStatus::done;
}

std::optional<Result<PolylineOffset>> PolylineSteps::joinAndRemove() {
    while (true) {
        if (m_live.empty()) {
            return vanishedOffset();
        }
        if (closed() && m_live.size() == 1) {
            return failedOffset(OffsetFailure::openJoint, m_simple);
        }
        if (const auto status = joinPending(); status != StepStatus::done) {
            return stopped(status, m_simple);
        }
        bool removed = false;
        for (auto& piece : m_live) {
            if (!piece.coursePending) {
                continue;
            }
            piece.coursePending = false;
            const auto course = detail::courseOf(m_source, piece.slot, piece.start, piece.end);
            if (course.status != StepStatus::done) {
                return stopped(course.status, m_simple);
            }
            piece.course = course.course;
            removed = removed || course.course == Course::backward;
        }
        if (!removed) {
            return std::nullopt;
        }
        // The pieces turned backwards in this round go together; the piece
        // before each gap they leave joins the piece after it again. Their
        // other ends keep the cuts they have (offset.hpp, step 3).
        m_simple = false;
        std::vector<LivePiece> kept;
        kept.reserve(m_live.size());
        for (auto& piece : m_live) {
            if (piece.course == Course::backward) {
                if (!kept.empty()) {
                    kept.back().jointPending = true;
                }
                continue;
            }
            kept.push_back(piece);
        }
        // In a closed polyline a gap at the front follows the last piece kept.
        if (closed() && !kept.empty() && m_live.front().course == Course::backward) {
            kept.back().jointPending = true;
        }
        m_live = std::move(kept);
    }
}

Result<PolylineOffset> PolylineSteps::run(const robust::TolerancePolicy& policy) {
    // Step 1: each piece moves; an arc whose radius is no longer positive
    // vanishes.
    for (std::size_t slot = 0; slot < m_source.pieces.size(); ++slot) {
        const auto& meaning = m_source.pieces[slot];
        if (const auto* arc = std::get_if<detail::ArcMeaning>(&meaning.support)) {
            if (detail::offsetRadiusSign(*arc, m_source.distance) != robust::Sign::positive) {
                m_simple = false;
                continue;
            }
        }
        LivePiece piece;
        piece.slot = slot;
        piece.source = m_sources[slot];
        piece.start = PointRecipe::movedEnd(slot, PieceEnd::start);
        piece.end = PointRecipe::movedEnd(slot, PieceEnd::end);
        m_live.push_back(piece);
    }
    // Steps 2 and 3.
    if (auto stop = joinAndRemove()) {
        return std::move(*stop);
    }
    return assemble(policy);
}

Result<PolylineOffset> PolylineSteps::assemble(const robust::TolerancePolicy& policy) {
    // Step 4: pieces reduced to a point go. Their ends are the same point, so
    // their neighbours already meet there; an arc joint they had stays.
    std::vector<ResultPiece> pieces;
    const std::size_t count = m_live.size();
    for (std::size_t k = 0; k < count; ++k) {
        const auto& piece = m_live[k];
        if (piece.course == Course::point) {
            m_simple = false;
        } else {
            ResultPiece result;
            result.start = piece.start;
            result.end = piece.end;
            result.origin = {PieceOrigin::source, piece.source};
            result.slot = piece.slot;
            const auto& meaning = m_source.pieces[piece.slot];
            const bool movedEnds = piece.start.kind == PointRecipe::Kind::moved &&
                                   piece.end.kind == PointRecipe::Kind::moved;
            if (std::holds_alternative<detail::LineMeaning>(meaning.support) || movedEnds) {
                // A straight piece keeps its bulge (0 or -0); a curved one that
                // was not cut keeps it too, as offsetCurve() does.
                result.bulge = std::get<PolylineSegment>(meaning.curve).bulge;
            } else {
                result.bulgePending = true;
            }
            pieces.push_back(result);
        }
        if (piece.jointAfter == JointKind::arc) {
            ResultPiece gap;
            gap.start = piece.end;
            gap.end = m_live[(k + 1) % count].start;
            gap.bulge = piece.gapBulge;
            gap.origin = {PieceOrigin::gap, vertexAfter(piece.source)};
            pieces.push_back(gap);
        }
    }
    if (pieces.empty()) {
        return vanishedOffset();
    }
    if (closed() && pieces.size() == 1) {
        return failedOffset(OffsetFailure::openJoint, m_simple);
    }
    PolylineOffset result;
    result.outcome = OffsetOutcome::done;
    result.simple = m_simple;
    result.polyline.closed = closed();
    for (const auto& piece : pieces) {
        const auto start = detail::roundedPoint(m_source, piece.start);
        if (start.status != StepStatus::done) {
            return stopped(start.status, m_simple);
        }
        double bulge = piece.bulge;
        if (piece.bulgePending) {
            const auto computed = detail::arcBulge(m_source, piece.slot, piece.start, piece.end);
            if (computed.status != StepStatus::done) {
                return stopped(computed.status, m_simple);
            }
            bulge = computed.bulge;
        }
        result.polyline.vertices.push_back({start.point, bulge});
        result.origins.push_back(piece.origin);
    }
    if (!closed()) {
        const auto end = detail::roundedPoint(m_source, pieces.back().end);
        if (end.status != StepStatus::done) {
            return stopped(end.status, m_simple);
        }
        result.polyline.vertices.push_back({end.point, 0.0});
    }
    return check(std::move(result), policy);
}

// True when the point coincides, under the rule of ADR 0009, with one of the
// shared vertices.
bool nearShared(Point2 point, const std::vector<Point2>& shared,
                const robust::TolerancePolicy& policy) {
    return std::ranges::any_of(shared,
                               [&](Point2 vertex) { return policy.coincident(point, vertex); });
}

// Two consecutive pieces may meet only at a shared vertex, or at points that
// coincide with it: every isolated point near a shared vertex, every shared
// stretch with both ends near the same one.
bool meetOnlyAtShared(const PairIntersections& meeting, const std::vector<Point2>& shared,
                      const robust::TolerancePolicy& policy) {
    for (const auto& point : meeting.points) {
        if (!nearShared(point.point, shared, policy)) {
            return false;
        }
    }
    for (const auto& overlap : meeting.overlaps) {
        if (overlap.whole) {
            return false;
        }
        bool inside = false;
        for (const auto vertex : shared) {
            inside = inside || (policy.coincident(overlap.from.point, vertex) &&
                                policy.coincident(overlap.to.point, vertex));
        }
        if (!inside) {
            return false;
        }
    }
    return true;
}

bool apart(const PairIntersections& meeting) {
    return meeting.points.empty() && meeting.overlaps.empty() && meeting.omittedBeyondDomain == 0;
}

Result<PolylineOffset> PolylineSteps::check(PolylineOffset result,
                                            const robust::TolerancePolicy& policy) const {
    const auto own = pieces(result.polyline);
    const std::size_t count = own.size();
    // A piece whose rounded ends are the same point has no extent of its own:
    // its neighbours share that point, and the pairs below see it. A piece
    // beyond the domain of the data (its point m, with a bulge near a whole
    // turn) cannot be returned.
    std::vector<bool> point(count, false);
    for (std::size_t i = 0; i < count; ++i) {
        if (const auto problem = detail::check(own[i], 0)) {
            if (problem->code != DiagnosisCode::degenerateCurve) {
                return failedOffset(OffsetFailure::beyondDomain, result.simple);
            }
            point[i] = true;
        }
    }
    const auto& vertices = result.polyline.vertices;
    for (std::size_t i = 0; i < count; ++i) {
        for (std::size_t j = i + 1; j < count && !point[i]; ++j) {
            if (point[j]) {
                continue;
            }
            std::vector<Point2> shared;
            if (j == i + 1) {
                shared.push_back(vertices[j].position);
            }
            if (closed() && i == 0 && j + 1 == count) {
                shared.push_back(vertices[0].position);
            }
            const auto meeting = intersect(own[i], Extension::none, own[j], Extension::none);
            const bool allowed =
                meeting.hasValue() &&
                (shared.empty() ? apart(meeting.value())
                                : meetOnlyAtShared(meeting.value(), shared, policy));
            if (!allowed) {
                return failedOffset(OffsetFailure::selfIntersection, result.simple);
            }
        }
    }
    const auto input = pieces(*m_polyline);
    for (std::size_t i = 0; i < count; ++i) {
        if (point[i]) {
            continue;
        }
        for (std::size_t j = 0; j < input.size(); ++j) {
            if (m_dropped[j]) {
                continue;
            }
            const auto meeting = intersect(own[i], Extension::none, input[j], Extension::none);
            if (!meeting.hasValue() || !apart(meeting.value())) {
                return failedOffset(OffsetFailure::meetsOriginal, result.simple);
            }
        }
    }
    return result;
}

bool goodPoint(Point2 point) {
    return std::isfinite(point.x) && std::isfinite(point.y) &&
           std::abs(point.x) <= robust::kMaxCoordinate &&
           std::abs(point.y) <= robust::kMaxCoordinate;
}

// The vertex that a diagnosis about piece i names: its end vertex when only
// the end is wrong, its start vertex otherwise (the start, the bulge, or the
// point m of the arc).
std::size_t vertexAt(const PolylineSegment& piece, std::size_t index, const Polyline& polyline) {
    if (goodPoint(piece.start) && std::isfinite(piece.bulge) && !goodPoint(piece.end)) {
        return (index + 1) % polyline.vertices.size();
    }
    return index;
}

}  // namespace

Result<PolylineOffset> offsetPolyline(const Polyline& polyline, double distance,
                                      const robust::TolerancePolicy& policy) {
    const auto input = pieces(polyline);
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (auto problem = detail::checkData(input[i], 0)) {
            problem->message = "vertex " + std::to_string(vertexAt(input[i], i, polyline)) + ": " +
                               problem->message;
            return std::move(*problem);
        }
    }
    // The first drop: pieces that are exactly a single point.
    OffsetSource source;
    std::vector<std::size_t> sources;
    std::vector<bool> dropped(input.size(), false);
    for (std::size_t i = 0; i < input.size(); ++i) {
        auto meaning = detail::meaningOf(input[i]);
        if (meaning.degenerate) {
            dropped[i] = true;
            continue;
        }
        source.pieces.push_back(std::move(meaning));
        sources.push_back(i);
    }
    if (source.pieces.empty()) {
        return detail::degenerateDiagnosis(0);
    }
    if (!std::isfinite(distance)) {
        return Diagnosis{DiagnosisCode::invalidValue, 1, "the offset distance is not finite"};
    }
    if (std::abs(distance) > robust::kMaxCoordinate) {
        return Diagnosis{DiagnosisCode::outOfDomain, 1, "the offset distance is beyond the domain"};
    }
    // A distance of zero would give the input itself (ADR 0012, point 2).
    if (exact::Dyadic(distance).isZero()) {
        return failedOffset(OffsetFailure::meetsOriginal, true);
    }
    source.distance = distance;
    PolylineSteps steps(polyline, std::move(sources), std::move(dropped), std::move(source));
    return steps.run(policy);
}

}  // namespace solid_space::geom2d
