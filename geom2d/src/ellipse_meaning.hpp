// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "solid_space/geom2d/ellipse.hpp"

#include <array>
#include <optional>
#include <span>

namespace solid_space::geom2d::detail {

/// The carrying ellipse is the image of the unit circle under
/// F(u, v) = center + u * major + v * minor. All entries are dyadic and
/// determinant = major x minor is positive. For q = p - center, the
/// implicit equation is (q x minor)^2 + (major x q)^2 - determinant^2 = 0.
/// The same two cross products give the inverse direction without division.
struct EllipseSupport {
    ExactVector center;
    ExactVector major;
    ExactVector minor;
    Dyadic determinant;
};

/// The arc on the unit circle uses the exact nominal sweep modulo kFullTurn
/// and the direction rules of Arc. unitArc.circle is the unit circle.
struct EllipseMeaning {
    EllipseSupport support;
    ArcMeaning unitArc;
};

/// The invalidValue and outOfDomain checks, in that order.
[[nodiscard]] std::optional<Diagnosis> checkEllipseData(const Ellipse& ellipse, std::size_t input);

/// Also rejects a zero axis or an arc reduced to one direction.
[[nodiscard]] Result<EllipseMeaning> checkedEllipse(const Ellipse& ellipse, std::size_t input);

/// F applied to the unit point in a nonzero dyadic direction. Use this with
/// unitArc.startDirection and unitArc.endDirection for the exact endpoints,
/// and nearestPoint() from positions.hpp to match endpoints() bit for bit.
[[nodiscard]] ExactPoint ellipsePoint(const EllipseSupport& support, const ExactVector& direction);

/// Sign of the implicit equation. Zero is on the carrier, negative inside.
[[nodiscard]] Sign ellipseSign(const EllipseSupport& support, const ExactPoint& point);

/// Exact position on the arc or its extension. Precondition: the point lies
/// on the carrying ellipse. A whole ellipse uses atStart and inside.
[[nodiscard]] Location locateOnEllipse(const EllipseMeaning& ellipse, const ExactPoint& point);

struct EllipseLinePoint {
    ExactPoint point;
    Contact contact = Contact::crossing;
    Location onEllipse = Location::inside;
    Location onSegment = Location::inside;
};

struct EllipseLineMeeting {
    std::array<EllipseLinePoint, 2> points{};
    std::size_t count = 0;

    [[nodiscard]] std::span<const EllipseLinePoint> found() const noexcept {
        return {points.data(), count};
    }
};

/// Exact intersections with the entire directed line in line order. A
/// double root gives one touching point. Locations are computed before
/// rounding, even outside the segment, arc or coordinate domain. The caller
/// can filter them with takesPart() and count beyondDomain() points where
/// required. Precondition: finite line endpoints that are distinct.
[[nodiscard]] EllipseLineMeeting meetEllipseLine(const EllipseMeaning& ellipse,
                                                 const LineMeaning& line);

}  // namespace solid_space::geom2d::detail
