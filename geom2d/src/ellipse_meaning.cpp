// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "ellipse_meaning.hpp"

#include "exact/quadratic.hpp"
#include "solid_space/robust/trigonometry.hpp"

#include <cassert>
#include <cmath>
#include <cstdint>

namespace solid_space::geom2d::detail {
namespace {

const Dyadic kOne{1.0};

Dyadic dot(const ExactVector& a, const ExactVector& b) {
    return a.x * b.x + a.y * b.y;
}

Dyadic cross(const ExactVector& a, const ExactVector& b) {
    return a.x * b.y - a.y * b.x;
}

ExactVector difference(const ExactVector& a, const ExactVector& b) {
    return {a.x - b.x, a.y - b.y};
}

ExactVector inverseNumerator(const EllipseSupport& support, const ExactVector& vector) {
    return {cross(vector, support.minor), cross(support.major, vector)};
}

ExactVector inverseNumerator(const EllipseSupport& support, const ExactPoint& point) {
    return inverseNumerator(support,
                            ExactVector{point.rational.x - support.center.x * point.weight,
                                        point.rational.y - support.center.y * point.weight});
}

bool finite(Point2 point) {
    return std::isfinite(point.x) && std::isfinite(point.y);
}

bool coordinateInDomain(Point2 point) {
    return std::fabs(point.x) <= robust::kMaxCoordinate &&
           std::fabs(point.y) <= robust::kMaxCoordinate;
}

Point2 directionAt(double parameter) {
    const auto direction = robust::sinCos(parameter);
    return {direction.cosine, direction.sine};
}

Dyadic nominalSweep(double start, double end) {
    const Dyadic turn{kFullTurn};
    const auto delta = Dyadic(end) - Dyadic(start);
    // In the angle domain the rounded quotient can change its floor by
    // at most one. Correct the residue exactly, mapping zero to a full turn.
    const auto turns = static_cast<std::int64_t>(std::floor(quotientToDouble(delta, turn)));
    const auto remainder = delta - Dyadic::fromInteger(turns) * turn;
    if (remainder.sign() != Sign::positive) {
        return remainder + turn;
    }
    return compare(remainder, turn) == Sign::positive ? remainder - turn : remainder;
}

double arcRuleSweep(const Dyadic& sweep) {
    // arcRule reads only the sign and these three thresholds. Use an exact
    // representative of each band to avoid rounding the sweep across a threshold.
    if (compare(sweep, Dyadic(kFullTurn)) == Sign::zero) {
        return kFullTurn;
    }
    if (compare(sweep, Dyadic(kThreeQuarterTurn)) != Sign::negative) {
        return kThreeQuarterTurn;
    }
    if (compare(sweep, Dyadic(kQuarterTurn)) != Sign::positive) {
        return kQuarterTurn;
    }
    return kFullTurn / 2.0;
}

Location segmentLocation(const Dyadic& numerator, const Dyadic& radical, const Dyadic& radicand,
                         const Dyadic& weight) {
    switch (exact::signOf(numerator, radical, radicand)) {
        case Sign::negative:
            return Location::beforeStart;
        case Sign::zero:
            return Location::atStart;
        case Sign::positive:
            break;
    }
    switch (exact::signOf(numerator - weight, radical, radicand)) {
        case Sign::negative:
            return Location::inside;
        case Sign::zero:
            return Location::atEnd;
        case Sign::positive:
            return Location::afterEnd;
    }
    return Location::afterEnd;
}

}  // namespace

std::optional<Diagnosis> checkEllipseData(const Ellipse& ellipse, std::size_t input) {
    if (!finite(ellipse.center) || !finite(ellipse.majorAxis) || !std::isfinite(ellipse.ratio) ||
        !std::isfinite(ellipse.startParameter) || !std::isfinite(ellipse.endParameter) ||
        ellipse.ratio <= 0.0 || ellipse.ratio > 1.0) {
        return Diagnosis{DiagnosisCode::invalidValue, input,
                         "an ellipse value is NaN or infinite, or its ratio is outside (0, 1]"};
    }
    if (!coordinateInDomain(ellipse.center) || !coordinateInDomain(ellipse.majorAxis) ||
        std::fabs(ellipse.startParameter) > robust::kMaxTrigArgument ||
        std::fabs(ellipse.endParameter) > robust::kMaxTrigArgument) {
        return Diagnosis{DiagnosisCode::outOfDomain, input,
                         "an ellipse coordinate or parameter is beyond its domain"};
    }
    return std::nullopt;
}

Result<EllipseMeaning> checkedEllipse(const Ellipse& ellipse, std::size_t input) {
    if (const auto diagnosis = checkEllipseData(ellipse, input)) {
        return *diagnosis;
    }
    const auto major = exactVector(ellipse.majorAxis);
    if (major.x.isZero() && major.y.isZero()) {
        return degenerateDiagnosis(input);
    }

    const auto sweep = nominalSweep(ellipse.startParameter, ellipse.endParameter);
    const auto start = directionAt(ellipse.startParameter);
    const auto end = directionAt(ellipse.endParameter);
    const auto rule = arcRule(arcRuleSweep(sweep), start, end);
    if (rule.span == ArcSpan::point) {
        return degenerateDiagnosis(input);
    }

    const Dyadic ratio{ellipse.ratio};
    const ExactVector minor{-ratio * major.y, ratio * major.x};
    return EllipseMeaning{
        {exactVector(ellipse.center), major, minor, cross(major, minor)},
        {CircleSupport{{}, {}, kOne, kOne}, rule.turn, rule.span, exactVector(start),
         exactVector(rule.span == ArcSpan::whole ? start : end), rule.endSector},
    };
}

ExactPoint ellipsePoint(const EllipseSupport& support, const ExactVector& direction) {
    const auto normSquared = dot(direction, direction);
    assert(normSquared.sign() == Sign::positive);
    // F(D / |D|) = center + A D sqrt(D . D) / (D . D).
    return {{support.center.x * normSquared, support.center.y * normSquared},
            {support.major.x * direction.x + support.minor.x * direction.y,
             support.major.y * direction.x + support.minor.y * direction.y},
            normSquared,
            normSquared};
}

Sign ellipseSign(const EllipseSupport& support, const ExactPoint& point) {
    const auto rational = inverseNumerator(support, point);
    const auto radical = inverseNumerator(support, point.radical);
    const auto scaledDeterminant = point.weight * support.determinant;
    return exact::signOf(dot(rational, rational) + dot(radical, radical) * point.radicand -
                             scaledDeterminant * scaledDeterminant,
                         dot(rational, radical).timesPowerOfTwo(1), point.radicand);
}

Location locateOnEllipse(const EllipseMeaning& ellipse, const ExactPoint& point) {
    const auto rational = inverseNumerator(ellipse.support, point);
    const auto radical = inverseNumerator(ellipse.support, point.radical);
    const auto& arc = ellipse.unitArc;
    const auto fromStart =
        sectorOf(exact::signOf(cross(arc.startDirection, rational),
                               cross(arc.startDirection, radical), point.radicand),
                 exact::signOf(dot(arc.startDirection, rational), dot(arc.startDirection, radical),
                               point.radicand));
    const auto toEnd = exact::signOf(cross(rational, arc.endDirection),
                                     cross(radical, arc.endDirection), point.radicand);
    return locateOnArc(arc, fromStart, toEnd);
}

EllipseLineMeeting meetEllipseLine(const EllipseMeaning& ellipse, const LineMeaning& line) {
    const auto start = exactVector(line.start);
    const auto direction = difference(exactVector(line.end), start);
    const auto& support = ellipse.support;
    const auto u = inverseNumerator(support, difference(start, support.center));
    const auto v = inverseNumerator(support, direction);
    // Substitute start + t * direction in the implicit equation. Use half
    // the linear coefficient so the roots are (-halfB +- sqrt(d)) / a.
    const auto a = dot(v, v);
    assert(a.sign() == Sign::positive);
    const auto halfB = dot(u, v);
    const auto c = dot(u, u) - support.determinant * support.determinant;
    const auto discriminant = halfB * halfB - a * c;
    EllipseLineMeeting result;
    if (discriminant.sign() == Sign::negative) {
        return result;
    }

    const ExactVector rational{start.x * a - direction.x * halfB,
                               start.y * a - direction.y * halfB};
    const auto makePoint = [&](const Dyadic& rootSign, Contact contact) {
        const ExactPoint point{
            rational, {direction.x * rootSign, direction.y * rootSign}, discriminant, a};
        return EllipseLinePoint{point, contact, locateOnEllipse(ellipse, point),
                                segmentLocation(-halfB, rootSign, discriminant, a)};
    };
    if (discriminant.isZero()) {
        result.points[0] = makePoint(Dyadic{}, Contact::touching);
        result.count = 1;
    } else {
        result.points = {makePoint(-kOne, Contact::crossing), makePoint(kOne, Contact::crossing)};
        result.count = result.points.size();
    }
    return result;
}

}  // namespace solid_space::geom2d::detail
