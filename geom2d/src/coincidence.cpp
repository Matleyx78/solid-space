// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "solid_space/geom2d/coincidence.hpp"

#include "curve_meaning.hpp"
#include "exact/quadratic.hpp"

#include <algorithm>
#include <cstddef>
#include <variant>

namespace solid_space::geom2d {
namespace {

using detail::ArcMeaning;
using detail::CircleSupport;
using detail::ExactVector;
using exact::Dyadic;
using robust::Sign;

Dyadic squared(const Dyadic& value) {
    return value * value;
}

Dyadic normSquared(const ExactVector& vector) {
    return squared(vector.x) + squared(vector.y);
}

ExactVector difference(Point2 a, Point2 b) {
    return {Dyadic(a.x) - Dyadic(b.x), Dyadic(a.y) - Dyadic(b.y)};
}

bool atMost(const Dyadic& a, const Dyadic& b) {
    return exact::compare(a, b) != Sign::positive;
}

bool nearLine(Point2 point, const Segment& segment, const Dyadic& epsilonSquared) {
    const auto direction = difference(segment.end, segment.start);
    const auto offset = difference(point, segment.start);
    const auto cross = direction.x * offset.y - direction.y * offset.x;
    // No normalization: cross^2 / |direction|^2 is the exact squared distance.
    return atMost(squared(cross), epsilonSquared * normSquared(direction));
}

bool nearCenters(const CircleSupport& a, const CircleSupport& b, const Dyadic& epsilonSquared) {
    const ExactVector delta{a.centerX * b.weight - b.centerX * a.weight,
                            a.centerY * b.weight - b.centerY * a.weight};
    return atMost(normSquared(delta), epsilonSquared * squared(a.weight * b.weight));
}

bool nearRadii(const CircleSupport& a, const CircleSupport& b, const Dyadic& epsilonSquared) {
    const auto first = a.radiusSquared * squared(b.weight);
    const auto second = b.radiusSquared * squared(a.weight);
    const auto limit = epsilonSquared * squared(a.weight * b.weight);
    // |sqrt(first) - sqrt(second)| <= sqrt(limit). Compare the squared
    // difference without squaring a possibly negative side of an inequality.
    return exact::signOf(first + second - limit, Dyadic(-2.0), first * second) != Sign::positive;
}

Result<detail::CurveMeaning> circularMeaning(const Curve& curve, std::size_t input) {
    auto result = detail::checkedMeaning(curve, input);
    if (!result.hasValue()) {
        return result.diagnosis();
    }
    if (std::holds_alternative<detail::LineMeaning>(result.value().support)) {
        return Diagnosis{DiagnosisCode::notCircular, input, "the curve is straight"};
    }
    return result;
}

Result<bool> compareCircles(const Curve& first, const Curve& second,
                            const robust::TolerancePolicy& policy, bool compareRadii) {
    const auto a = circularMeaning(first, 0);
    if (!a.hasValue()) {
        return a.diagnosis();
    }
    const auto b = circularMeaning(second, 1);
    if (!b.hasValue()) {
        return b.diagnosis();
    }
    const double size = std::max(a.value().magnitude, b.value().magnitude);
    const auto epsilonSquared = squared(Dyadic(policy.epsilon(size)));
    const auto& circleA = std::get<ArcMeaning>(a.value().support).circle;
    const auto& circleB = std::get<ArcMeaning>(b.value().support).circle;
    return nearCenters(circleA, circleB, epsilonSquared) &&
           (!compareRadii || nearRadii(circleA, circleB, epsilonSquared));
}

bool shortArc(const ArcMeaning& arc, const Dyadic& epsilonSquared) {
    const auto& circle = arc.circle;
    if (arc.span != detail::ArcSpan::partial || arc.endSector > detail::TurnSector::half) {
        return atMost(circle.radiusSquared.timesPowerOfTwo(2),
                      epsilonSquared * squared(circle.weight));
    }
    // Curved polyline segments have exact rational endpoints. For Arc the
    // endpoints are c + r D/|D|: chord^2 = 2 r^2 (1 - dot(D0,D1)/sqrt(N)).
    const auto& start = arc.startDirection;
    const auto& end = arc.endDirection;
    const auto dot = start.x * end.x + start.y * end.y;
    const auto normProduct = normSquared(start) * normSquared(end);
    const auto twiceRadiusSquared = circle.radiusSquared.timesPowerOfTwo(1);
    return exact::signOf(-twiceRadiusSquared * dot,
                         twiceRadiusSquared - epsilonSquared * squared(circle.weight),
                         normProduct) != Sign::positive;
}

}  // namespace

Result<bool> isDegenerate(const Curve& curve, const robust::TolerancePolicy& policy) {
    const auto result = detail::checkedMeaning(curve, 0, detail::Degenerate::accept);
    if (!result.hasValue()) {
        return result.diagnosis();
    }
    const auto& meaning = result.value();
    if (meaning.degenerate) {
        return true;
    }
    const auto epsilonSquared = squared(Dyadic(policy.epsilon(meaning.magnitude)));
    if (const auto* segment = std::get_if<Segment>(&curve)) {
        return atMost(normSquared(difference(segment->end, segment->start)), epsilonSquared);
    }
    if (const auto* piece = std::get_if<PolylineSegment>(&curve)) {
        // A bulge of magnitude at most one spans at most half a turn.
        if (atMost(Dyadic(piece->bulge).abs(), Dyadic(1.0))) {
            return atMost(normSquared(difference(piece->end, piece->start)), epsilonSquared);
        }
    }
    return shortArc(std::get<ArcMeaning>(meaning.support), epsilonSquared);
}

Result<bool> isStraight(const PolylineSegment& segment, const robust::TolerancePolicy& policy) {
    if (const auto diagnosis = detail::checkData(segment, 0)) {
        return *diagnosis;
    }
    const auto epsilonSquared = squared(Dyadic(policy.epsilon(detail::magnitudeOf(segment))));
    // sagitta^2 = bulge^2 * chord^2 / 4, including zero bulge and zero chord.
    const auto chordSquared = normSquared(difference(segment.end, segment.start));
    const auto sagittaSquaredTimesFour = squared(Dyadic(segment.bulge)) * chordSquared;
    return atMost(sagittaSquaredTimesFour, epsilonSquared.timesPowerOfTwo(2));
}

Result<bool> areAligned(const Segment& first, const Segment& second,
                        const robust::TolerancePolicy& policy) {
    if (const auto diagnosis = detail::check(first, 0)) {
        return *diagnosis;
    }
    if (const auto diagnosis = detail::check(second, 1)) {
        return *diagnosis;
    }
    const double size = std::max(detail::magnitudeOf(first), detail::magnitudeOf(second));
    const auto epsilonSquared = squared(Dyadic(policy.epsilon(size)));
    return nearLine(first.start, second, epsilonSquared) &&
           nearLine(first.end, second, epsilonSquared) &&
           nearLine(second.start, first, epsilonSquared) &&
           nearLine(second.end, first, epsilonSquared);
}

Result<bool> areConcentric(const Curve& first, const Curve& second,
                           const robust::TolerancePolicy& policy) {
    return compareCircles(first, second, policy, false);
}

Result<bool> onSameCircle(const Curve& first, const Curve& second,
                          const robust::TolerancePolicy& policy) {
    return compareCircles(first, second, policy, true);
}

}  // namespace solid_space::geom2d
