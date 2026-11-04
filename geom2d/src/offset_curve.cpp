// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// offsetCurve (offset.hpp; ADR 0012, point 3; stage 0.2, packages P13 and P19).
//
// The ends of a Segment or PolylineSegment are moved and rounded as
// offsetPolyline() moves and rounds them (offset_joints.hpp, roundedPoint()),
// so that a polyline of one piece gives the same bits. The order of the
// answers follows offset.hpp:
// 1. the data (curves.hpp), then the distance;
// 2. vanished for a circular curve whose new radius is not positive;
// 3. outOfDomain (input 1) for an exact constructed end or radius beyond
//    kMaxCoordinate, before any failed (#153);
// 4. failed for rounded ends that coincide, or for an arc that its rounded
//    ends and the bulge of the rule cannot represent (representArc());
// 5. outOfDomain (input 1) for the point m of a returned curved piece,
//    reconstructed from its doubles.
// Only data values are checked against the domain, never the geometric extent
// of the curve (#144): a circle or an arc keeps its centre, so only its radius
// is checked.

#include "curve_meaning.hpp"
#include "exact/dyadic.hpp"
#include "offset_carriers.hpp"
#include "offset_joints.hpp"
#include "solid_space/geom2d/offset.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <variant>

namespace solid_space::geom2d {
namespace {

using detail::OffsetSource;
using detail::PieceEnd;
using detail::PointRecipe;
using detail::StepStatus;
using exact::Dyadic;
using robust::Sign;

Diagnosis beyondDomain() {
    return {DiagnosisCode::outOfDomain, 1, "a point of the offset is beyond the coordinate domain"};
}

Diagnosis towerFull() {
    return Diagnosis{DiagnosisCode::outOfDomain, 0,
                     "the exact numbers of the offset need more roots than a tower holds"};
}

CurveOffset failedCurve() {
    CurveOffset result;
    result.outcome = OffsetOutcome::failed;
    return result;
}

bool coincide(Point2 a, Point2 b) {
    return a.x == b.x && a.y == b.y;
}

// A Segment or a PolylineSegment, as the single piece of an offset source.
Result<CurveOffset> offsetPiece(const Curve& curve, detail::CurveMeaning meaning, double distance) {
    const bool straight = std::holds_alternative<detail::LineMeaning>(meaning.support);
    const double magnitude = std::max(meaning.magnitude, std::abs(distance));
    if (std::holds_alternative<Segment>(curve)) {
        // The meaning of the same segment written as a straight piece.
        const auto& segment = std::get<Segment>(curve);
        meaning = detail::meaningOf(PolylineSegment{segment.start, segment.end, 0.0});
    }
    OffsetSource source;
    source.pieces.push_back(std::move(meaning));
    source.distance = distance;
    source.magnitude = magnitude;
    const auto start = detail::roundedPoint(source, PointRecipe::movedEnd(0, PieceEnd::start));
    const auto end = detail::roundedPoint(source, PointRecipe::movedEnd(0, PieceEnd::end));
    if (start.status != StepStatus::done || end.status != StepStatus::done) {
        return towerFull();
    }
    if (start.beyondDomain || end.beyondDomain) {
        return beyondDomain();
    }
    if (coincide(start.point, end.point)) {
        return failedCurve();
    }
    if (straight) {
        if (const auto* piece = std::get_if<PolylineSegment>(&curve)) {
            return CurveOffset{OffsetOutcome::done,
                               PolylineSegment{start.point, end.point, piece->bulge}};
        }
        return CurveOffset{OffsetOutcome::done, Segment{start.point, end.point}};
    }
    detail::OffsetScope scope(source);
    const auto arc = detail::sourceArc(scope, 0, PointRecipe::movedEnd(0, PieceEnd::start),
                                       PointRecipe::movedEnd(0, PieceEnd::end));
    if (!arc) {
        return towerFull();
    }
    const auto representation =
        detail::representArc(scope.tower(), *arc, start.point, end.point, magnitude);
    if (!representation.representable) {
        return failedCurve();
    }
    const PolylineSegment result{start.point, end.point, representation.bulge};
    if (detail::checkData(result, 1)) {
        return beyondDomain();
    }
    return CurveOffset{OffsetOutcome::done, result};
}

// An Arc or a Circle: the same centre, the radius changed exactly.
Result<CurveOffset> offsetRound(const Curve& curve, const detail::ArcMeaning& arc,
                                const Dyadic& distance) {
    const auto inward = arc.turn == detail::Turn::counterclockwise ? distance : -distance;
    const double oldRadius = std::holds_alternative<Arc>(curve) ? std::get<Arc>(curve).radius
                                                                : std::get<Circle>(curve).radius;
    const auto radius = Dyadic(oldRadius) - inward;
    if (radius.sign() != Sign::positive) {
        return CurveOffset{};
    }
    if (compare(radius, Dyadic(robust::kMaxCoordinate)) == Sign::positive) {
        return beyondDomain();
    }
    // The nearest double of an exact radius within kMaxCoordinate is within
    // it too: kMaxCoordinate is a double.
    if (const auto* input = std::get_if<Arc>(&curve)) {
        auto result = *input;
        result.radius = radius.toDouble();
        return CurveOffset{OffsetOutcome::done, result};
    }
    auto result = std::get<Circle>(curve);
    result.radius = radius.toDouble();
    return CurveOffset{OffsetOutcome::done, result};
}

}  // namespace

Result<CurveOffset> offsetCurve(const Curve& curve, double distance) {
    auto meaning = detail::checkedMeaning(curve, 0);
    if (!meaning.hasValue()) {
        return meaning.diagnosis();
    }
    if (!std::isfinite(distance)) {
        return Diagnosis{DiagnosisCode::invalidValue, 1, "the offset distance is not finite"};
    }
    if (std::abs(distance) > robust::kMaxCoordinate) {
        return Diagnosis{DiagnosisCode::outOfDomain, 1, "the offset distance is beyond the domain"};
    }
    const Dyadic exactDistance(distance);
    if (exactDistance.isZero()) {
        return CurveOffset{OffsetOutcome::done, curve};
    }
    if (std::holds_alternative<Arc>(curve) || std::holds_alternative<Circle>(curve)) {
        return offsetRound(curve, std::get<detail::ArcMeaning>(meaning.value().support),
                           exactDistance);
    }
    if (const auto* arc = std::get_if<detail::ArcMeaning>(&meaning.value().support)) {
        if (detail::offsetRadiusSign(*arc, distance) != Sign::positive) {
            return CurveOffset{};
        }
    }
    return offsetPiece(curve, std::move(meaning).value(), distance);
}

}  // namespace solid_space::geom2d
