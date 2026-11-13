// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "solid_space/geom2d/ellipse.hpp"

#include "ellipse_meaning.hpp"
#include "positions.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace solid_space::geom2d {
namespace {

using robust::Sign;

std::optional<Diagnosis> checkCorner(Point2 point, std::size_t input) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
        return Diagnosis{DiagnosisCode::invalidValue, input,
                         "a coordinate of the point is NaN or infinite"};
    }
    if (std::fabs(point.x) > robust::kMaxCoordinate ||
        std::fabs(point.y) > robust::kMaxCoordinate) {
        return Diagnosis{DiagnosisCode::outOfDomain, input,
                         "a coordinate of the point is beyond robust::kMaxCoordinate"};
    }
    return std::nullopt;
}

bool contains(const detail::ExactPoint& point, const detail::ExactPoint& minimum,
              const detail::ExactPoint& maximum) {
    return detail::compareX(point, minimum) != Sign::negative &&
           detail::compareX(point, maximum) != Sign::positive &&
           detail::compareY(point, minimum) != Sign::negative &&
           detail::compareY(point, maximum) != Sign::positive;
}

bool onCurve(Location location) {
    return detail::takesPart(location, Extension::none);
}

bool meetsSide(const detail::EllipseMeaning& ellipse, const detail::LineMeaning& side) {
    const auto meetings = detail::meetEllipseLine(ellipse, side);
    return std::ranges::any_of(meetings.found(), [](const auto& meeting) {
        return onCurve(meeting.onEllipse) && onCurve(meeting.onSegment);
    });
}

}  // namespace

Result<double> magnitude(const Ellipse& ellipse) {
    if (const auto diagnosis = detail::checkEllipseData(ellipse, 0)) {
        return *diagnosis;
    }
    return std::max({std::fabs(ellipse.center.x), std::fabs(ellipse.center.y),
                     std::fabs(ellipse.majorAxis.x), std::fabs(ellipse.majorAxis.y)});
}

Result<Endpoints> endpoints(const Ellipse& ellipse) {
    const auto meaning = detail::checkedEllipse(ellipse, 0);
    if (!meaning.hasValue()) {
        return meaning.diagnosis();
    }
    const auto& [support, arc] = meaning.value();
    const auto start = detail::nearestPoint(detail::ellipsePoint(support, arc.startDirection));
    const auto end = arc.span == detail::ArcSpan::whole
                         ? start
                         : detail::nearestPoint(detail::ellipsePoint(support, arc.endDirection));
    return Endpoints{start, end};
}

Result<bool> meetsRectangle(const Ellipse& ellipse, Point2 corner, Point2 oppositeCorner) {
    const auto meaning = detail::checkedEllipse(ellipse, 0);
    if (!meaning.hasValue()) {
        return meaning.diagnosis();
    }
    if (const auto diagnosis = checkCorner(corner, 1)) {
        return *diagnosis;
    }
    if (const auto diagnosis = checkCorner(oppositeCorner, 2)) {
        return *diagnosis;
    }

    const auto& data = meaning.value();
    const Point2 minimum{std::min(corner.x, oppositeCorner.x),
                         std::min(corner.y, oppositeCorner.y)};
    const Point2 maximum{std::max(corner.x, oppositeCorner.x),
                         std::max(corner.y, oppositeCorner.y)};
    const auto minPoint = detail::exactPoint(minimum);
    const auto maxPoint = detail::exactPoint(maximum);
    const bool zeroWidth = detail::compareX(minPoint, maxPoint) == Sign::zero;
    const bool zeroHeight = detail::compareY(minPoint, maxPoint) == Sign::zero;
    if (zeroWidth && zeroHeight) {
        return detail::ellipseSign(data.support, minPoint) == Sign::zero &&
               onCurve(detail::locateOnEllipse(data, minPoint));
    }
    const auto startPoint = detail::ellipsePoint(data.support, data.unitArc.startDirection);
    if (contains(startPoint, minPoint, maxPoint)) {
        return true;
    }
    if (zeroWidth || zeroHeight) {
        return meetsSide(data, {minimum, maximum});
    }

    const Point2 lowerRight{maximum.x, minimum.y};
    const Point2 upperLeft{minimum.x, maximum.y};
    const std::array<detail::LineMeaning, 4> sides{
        {{minimum, lowerRight}, {lowerRight, maximum}, {maximum, upperLeft}, {upperLeft, minimum}}};
    return std::ranges::any_of(sides, [&data](const auto& side) { return meetsSide(data, side); });
}

}  // namespace solid_space::geom2d
