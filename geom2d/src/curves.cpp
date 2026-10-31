// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// pieces, magnitude, endpoints and length of curves.hpp (stage 0.2, package
// P7). The data are checked and given their exact meaning in curve_meaning.cpp;
// here the constructions are rounded within the errors that curves.hpp
// declares.
//
// Error budget, relative to max(M, |value|):
// - a point of an arc is (centre * n + radius * D * sqrt(n)) / n with
//   n = |D|^2, rounded once by quadraticToDouble (at most one ulp, 2^-52),
//   far below kRelativePointError (2^-48);
// - a length is a product of at most three factors, each within two ulp of
//   its exact value (sqrt by quadraticToDouble, atan2 of robust, the double
//   nearest to 2 pi), so it stays within about 2^-49, below
//   kRelativeStationError (2^-46).

#include "solid_space/geom2d/curves.hpp"

#include "curve_meaning.hpp"
#include "exact/quadratic.hpp"
#include "solid_space/robust/trigonometry.hpp"

#include <cmath>
#include <cstddef>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace solid_space::geom2d {
namespace {

using detail::ArcMeaning;
using detail::ArcSpan;
using detail::CurveMeaning;
using detail::ExactVector;
using detail::LineMeaning;
using detail::Turn;
using exact::Dyadic;

// Below this |bulge|, atan(|b|) / |b| = 1 - b^2 / 3 + ... is 1 within
// 2^-55: the quotient of robust::atan2 by |b| would add rounding for nothing,
// and for subnormal bulges it would lose the relative accuracy of atan2.
constexpr double kSmallBulge = 0x1p-27;

Dyadic squaredLength(const Dyadic& dx, const Dyadic& dy) {
    return dx * dx + dy * dy;
}

// The point of the circle (center, radius) in the direction of the vector
// `direction`: center + radius * direction / |direction|.
Point2 pointInDirection(Point2 center, double radius, const ExactVector& direction) {
    const Dyadic norm = squaredLength(direction.x, direction.y);
    const Dyadic r(radius);
    return Point2{
        exact::quadraticToDouble(Dyadic(center.x) * norm, r * direction.x, norm, norm),
        exact::quadraticToDouble(Dyadic(center.y) * norm, r * direction.y, norm, norm),
    };
}

// The length of the straight segment from start to end.
double distance(Point2 start, Point2 end) {
    const Dyadic dx = Dyadic(end.x) - Dyadic(start.x);
    const Dyadic dy = Dyadic(end.y) - Dyadic(start.y);
    return exact::quadraticToDouble(Dyadic(), Dyadic(1.0), squaredLength(dx, dy), Dyadic(1.0));
}

// alpha: the angle from D0 to D1 of a partial arc, in the direction of the
// arc, in (0, 2 pi). The cross and dot products are exact and rounded once,
// which moves the angle by at most 2^-53; robust::atan2 adds two ulp.
double partialTurn(const ArcMeaning& arc) {
    const auto& d0 = arc.startDirection;
    const auto& d1 = arc.endDirection;
    Dyadic cross = d0.x * d1.y - d0.y * d1.x;
    if (arc.turn == Turn::clockwise) {
        cross = -cross;
    }
    const Dyadic dot = d0.x * d1.x + d0.y * d1.y;
    // An exact zero is +0, so that atan2 gives pi, not -pi, for half a turn.
    const double y = cross.isZero() ? 0.0 : cross.toDouble();
    const double angle = robust::atan2(y, dot.toDouble());
    return angle < 0.0 ? angle + kFullTurn : angle;
}

// The length of a curved polyline segment with bulge b and chord L:
// radius * angle = L (1 + b^2) / (4 |b|) * 4 atan(|b|), arranged so that no
// factor overflows or loses its relative accuracy.
double curvedPieceLength(const PolylineSegment& piece) {
    const Dyadic dx = Dyadic(piece.end.x) - Dyadic(piece.start.x);
    const Dyadic dy = Dyadic(piece.end.y) - Dyadic(piece.start.y);
    const double size = std::fabs(piece.bulge);
    const Dyadic bulge(size);
    const Dyadic plusOne = bulge * bulge + Dyadic(1.0);
    const Dyadic chord = squaredLength(dx, dy);
    if (size >= 1.0) {
        // L (1 + b^2) / |b| is at most about 2^53: the point m of the arc is
        // within the domain, and |b| L / 2 is its distance from the chord.
        const double scaled = exact::quadraticToDouble(Dyadic(), plusOne, chord, bulge);
        return scaled * robust::atan2(size, 1.0);
    }
    const double scaled = exact::quadraticToDouble(Dyadic(), plusOne, chord, Dyadic(1.0));
    const double ratio = size < kSmallBulge ? 1.0 : robust::atan2(size, 1.0) / size;
    return scaled * ratio;
}

Endpoints arcEndpoints(const Arc& data, const ArcMeaning& arc) {
    const auto start = pointInDirection(data.center, data.radius, arc.startDirection);
    if (arc.span == ArcSpan::whole) {
        return Endpoints{start, start};
    }
    return Endpoints{start, pointInDirection(data.center, data.radius, arc.endDirection)};
}

Endpoints roundedEndpoints(const CurveMeaning& meaning) {
    return std::visit(
        [&meaning](const auto& curve) -> Endpoints {
            using Type = std::decay_t<decltype(curve)>;
            if constexpr (std::is_same_v<Type, Circle>) {
                // One correctly rounded sum.
                const Point2 start{curve.center.x + curve.radius, curve.center.y};
                return Endpoints{start, start};
            } else if constexpr (std::is_same_v<Type, Arc>) {
                return arcEndpoints(curve, std::get<ArcMeaning>(meaning.support));
            } else {
                // Segment and PolylineSegment end at their data.
                return Endpoints{curve.start, curve.end};
            }
        },
        meaning.curve);
}

double roundedLength(const CurveMeaning& meaning) {
    return std::visit(
        [&meaning](const auto& curve) -> double {
            using Type = std::decay_t<decltype(curve)>;
            if constexpr (std::is_same_v<Type, Segment>) {
                return distance(curve.start, curve.end);
            } else if constexpr (std::is_same_v<Type, Circle>) {
                return kFullTurn * curve.radius;
            } else if constexpr (std::is_same_v<Type, Arc>) {
                const auto& arc = std::get<ArcMeaning>(meaning.support);
                if (arc.span == ArcSpan::whole) {
                    return kFullTurn * curve.radius;
                }
                return curve.radius * partialTurn(arc);
            } else {
                if (std::holds_alternative<LineMeaning>(meaning.support)) {
                    return distance(curve.start, curve.end);
                }
                return curvedPieceLength(curve);
            }
        },
        meaning.curve);
}

}  // namespace

std::vector<PolylineSegment> pieces(const Polyline& polyline) {
    const auto& vertices = polyline.vertices;
    std::vector<PolylineSegment> result;
    if (vertices.size() < 2) {
        return result;
    }
    const std::size_t count = polyline.closed ? vertices.size() : vertices.size() - 1;
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto& from = vertices[i];
        const auto& to = vertices[(i + 1) % vertices.size()];
        result.push_back(PolylineSegment{from.position, to.position, from.bulge});
    }
    return result;
}

Result<double> magnitude(const Curve& curve) {
    if (auto problem = detail::checkData(curve, 0)) {
        return std::move(*problem);
    }
    return detail::magnitudeOf(curve);
}

Result<Endpoints> endpoints(const Curve& curve) {
    if (auto problem = detail::checkData(curve, 0)) {
        return std::move(*problem);
    }
    const auto meaning = detail::meaningOf(curve);
    if (meaning.degenerate) {
        return detail::degenerateDiagnosis(0);
    }
    return roundedEndpoints(meaning);
}

Result<double> length(const Curve& curve) {
    if (auto problem = detail::checkData(curve, 0)) {
        return std::move(*problem);
    }
    const auto meaning = detail::meaningOf(curve);
    if (meaning.degenerate) {
        return detail::degenerateDiagnosis(0);
    }
    return roundedLength(meaning);
}

}  // namespace solid_space::geom2d
