// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "curve_meaning.hpp"
#include "exact/quadratic.hpp"
#include "solid_space/geom2d/offset.hpp"

#include <array>
#include <cmath>
#include <optional>
#include <variant>

namespace solid_space::geom2d {
namespace {

using detail::ExactVector;
using exact::Dyadic;
using robust::Sign;

Dyadic normSquared(const ExactVector& v) {
    return v.x * v.x + v.y * v.y;
}

Diagnosis beyondDomain() {
    return {DiagnosisCode::outOfDomain, 1, "a point of the offset is beyond the coordinate domain"};
}

// All denominators here are positive. Test the exact coordinate before rounding,
// including values just outside the domain that would round onto its boundary.
bool inDomain(const Dyadic& a, const Dyadic& b, const Dyadic& d, const Dyadic& c) {
    const auto limit = Dyadic(robust::kMaxCoordinate) * c;
    return exact::signOf(a - limit, b, d) != Sign::positive &&
           exact::signOf(a + limit, b, d) != Sign::negative;
}

Result<Point2> constructPoint(const ExactVector& a, const ExactVector& b, const Dyadic& d,
                              const Dyadic& c) {
    if (!inDomain(a.x, b.x, d, c) || !inDomain(a.y, b.y, d, c)) {
        return beyondDomain();
    }
    return Point2{exact::quadraticToDouble(a.x, b.x, d, c),
                  exact::quadraticToDouble(a.y, b.y, d, c)};
}

Result<Point2> moveOnNormal(Point2 p, const ExactVector& normal, const Dyadic& distance,
                            const Dyadic& squaredLength) {
    // p + distance * normal / sqrt(squaredLength), without underflow or
    // cancellation in a floating-point normalization.
    return constructPoint({Dyadic(p.x) * squaredLength, Dyadic(p.y) * squaredLength},
                          {distance * normal.x, distance * normal.y}, squaredLength, squaredLength);
}

Result<CurveOffset> offsetLine(const Curve& curve, const detail::LineMeaning& line,
                               const Dyadic& distance) {
    const ExactVector normal{Dyadic(line.start.y) - Dyadic(line.end.y),
                             Dyadic(line.end.x) - Dyadic(line.start.x)};
    const auto squaredLength = normSquared(normal);
    const auto start = moveOnNormal(line.start, normal, distance, squaredLength);
    if (!start.hasValue()) {
        return start.diagnosis();
    }
    const auto end = moveOnNormal(line.end, normal, distance, squaredLength);
    if (!end.hasValue()) {
        return end.diagnosis();
    }
    if (const auto* piece = std::get_if<PolylineSegment>(&curve)) {
        return CurveOffset{OffsetOutcome::done,
                           PolylineSegment{start.value(), end.value(), piece->bulge}};
    }
    return CurveOffset{OffsetOutcome::done, Segment{start.value(), end.value()}};
}

// Only extrema that belong to the actual arc count: a supporting circle may
// extend beyond the domain while the offset arc stays entirely inside it.
std::optional<Diagnosis> checkExtrema(const detail::ArcMeaning& arc, const Dyadic& inward) {
    const auto& circle = arc.circle;
    const std::array<ExactVector, 4> axes{
        ExactVector{Dyadic(1.0), Dyadic()}, ExactVector{Dyadic(-1.0), Dyadic()},
        ExactVector{Dyadic(), Dyadic(1.0)}, ExactVector{Dyadic(), Dyadic(-1.0)}};
    for (const auto& axis : axes) {
        if (detail::locateDirection(arc, axis) == Location::afterEnd) {
            continue;
        }
        const ExactVector a{circle.centerX - inward * circle.weight * axis.x,
                            circle.centerY - inward * circle.weight * axis.y};
        if (!inDomain(a.x, axis.x, circle.radiusSquared, circle.weight) ||
            !inDomain(a.y, axis.y, circle.radiusSquared, circle.weight)) {
            return beyondDomain();
        }
    }
    return std::nullopt;
}

Result<CurveOffset> offsetCircular(const Curve& curve, const detail::ArcMeaning& arc,
                                   const Dyadic& distance) {
    const auto inward = arc.turn == detail::Turn::counterclockwise ? distance : -distance;
    const auto& circle = arc.circle;
    // R - inward = (sqrt(radiusSquared) - inward * weight) / weight.
    if (exact::signOf(-inward * circle.weight, Dyadic(1.0), circle.radiusSquared) !=
        Sign::positive) {
        return CurveOffset{};
    }
    if (const auto diagnosis = checkExtrema(arc, inward)) {
        return *diagnosis;
    }
    if (const auto* piece = std::get_if<PolylineSegment>(&curve)) {
        const auto move = [&](Point2 p) {
            const ExactVector radial{Dyadic(p.x) * circle.weight - circle.centerX,
                                     Dyadic(p.y) * circle.weight - circle.centerY};
            return moveOnNormal(p, radial, -inward, circle.radiusSquared);
        };
        const auto start = move(piece->start);
        if (!start.hasValue()) {
            return start.diagnosis();
        }
        const auto end = move(piece->end);
        if (!end.hasValue()) {
            return end.diagnosis();
        }
        return CurveOffset{OffsetOutcome::done,
                           PolylineSegment{start.value(), end.value(), piece->bulge}};
    }
    if (const auto* inputArc = std::get_if<Arc>(&curve)) {
        const auto radius = Dyadic(inputArc->radius) - inward;
        // Arc directions are not assumed to have unit norm: the public meaning
        // normalizes the exact doubles of sinCos, not their rounded length.
        for (const auto& direction : {arc.startDirection, arc.endDirection}) {
            const auto squaredLength = normSquared(direction);
            const ExactVector center{Dyadic(inputArc->center.x) * squaredLength,
                                     Dyadic(inputArc->center.y) * squaredLength};
            const ExactVector radial{radius * direction.x, radius * direction.y};
            const auto point = constructPoint(center, radial, squaredLength, squaredLength);
            if (!point.hasValue()) {
                return point.diagnosis();
            }
        }
        auto result = *inputArc;
        result.radius = radius.toDouble();
        return CurveOffset{OffsetOutcome::done, result};
    }
    auto result = std::get<Circle>(curve);
    result.radius = (Dyadic(result.radius) - inward).toDouble();
    return CurveOffset{OffsetOutcome::done, result};
}

}  // namespace

Result<CurveOffset> offsetCurve(const Curve& curve, double distance) {
    const auto meaning = detail::checkedMeaning(curve, 0);
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
    if (const auto* line = std::get_if<detail::LineMeaning>(&meaning.value().support)) {
        return offsetLine(curve, *line, exactDistance);
    }
    return offsetCircular(curve, std::get<detail::ArcMeaning>(meaning.value().support),
                          exactDistance);
}

}  // namespace solid_space::geom2d
