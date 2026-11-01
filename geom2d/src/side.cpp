// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// The side of a point with respect to the carrier of a curve (side.hpp; stage
// 0.2, package P9). Exact, with no tolerance: the sign comes from
// robust::orient2d for a straight curve and from the same comparison that
// carriers.hpp uses for a circular one, so side() gives on exactly when an
// exact point lies on the carrier.

#include "solid_space/geom2d/side.hpp"

#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "solid_space/robust/point.hpp"
#include "solid_space/robust/predicates.hpp"

#include <cmath>
#include <optional>
#include <variant>

namespace solid_space::geom2d {
namespace {

using robust::Sign;

// The point is input 1 (side.hpp).
constexpr std::size_t kPointInput = 1;

std::optional<Diagnosis> checkPoint(Point2 point) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
        return Diagnosis{DiagnosisCode::invalidValue, kPointInput,
                         "a coordinate of the point is NaN or infinite"};
    }
    if (std::fabs(point.x) > robust::kMaxCoordinate ||
        std::fabs(point.y) > robust::kMaxCoordinate) {
        return Diagnosis{DiagnosisCode::outOfDomain, kPointInput,
                         "a coordinate of the point is beyond robust::kMaxCoordinate"};
    }
    return std::nullopt;
}

Side sideOf(Sign sign) noexcept {
    switch (sign) {
        case Sign::positive:
            return Side::left;
        case Sign::negative:
            return Side::right;
        case Sign::zero:
            break;
    }
    return Side::on;
}

}  // namespace

Result<Side> side(const Curve& curve, Point2 point) {
    auto meaning = detail::checkedMeaning(curve, 0);
    if (!meaning.hasValue()) {
        return meaning.diagnosis();
    }
    if (const auto diagnosis = checkPoint(point)) {
        return *diagnosis;
    }
    const auto& support = meaning.value().support;
    if (const auto* line = std::get_if<detail::LineMeaning>(&support)) {
        return sideOf(robust::orient2d(line->start, line->end, point));
    }
    return sideOf(detail::sideSign(meaning.value(), detail::exactPoint(point)));
}

}  // namespace solid_space::geom2d
