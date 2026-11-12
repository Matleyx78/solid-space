// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// The exact order of the pieces that leave a point (local_order.hpp; stage
// 0.3, package PS2).

#include "local_order.hpp"

#include "exact/quadratic.hpp"

#include <algorithm>
#include <cassert>
#include <numeric>
#include <utility>
#include <variant>

namespace solid_space::geom2d::detail {
namespace {

Sign negated(Sign sign) noexcept {
    switch (sign) {
        case Sign::negative:
            return Sign::positive;
        case Sign::positive:
            return Sign::negative;
        case Sign::zero:
            break;
    }
    return Sign::zero;
}

ExactVector minus(const ExactVector& a, const ExactVector& b) {
    return {a.x - b.x, a.y - b.y};
}

ExactVector times(const ExactVector& vector, const Dyadic& factor) {
    return {vector.x * factor, vector.y * factor};
}

ExactVector negated(const ExactVector& vector) {
    return {-vector.x, -vector.y};
}

// The vector turned by a quarter turn counterclockwise.
ExactVector perpendicular(const ExactVector& vector) {
    return {-vector.y, vector.x};
}

// A line: the tangent is the direction of the line in the heading, with no
// radical, and the curvature is zero.
Departure lineDeparture(const ExactPoint& point, const LineMeaning& line, Heading heading) {
    ExactVector direction = minus(exactVector(line.end), exactVector(line.start));
    assert(!(direction.x.isZero() && direction.y.isZero()) && "a line needs start != end");
    if (heading == Heading::backward) {
        direction = negated(direction);
    }
    return Departure{std::move(direction), ExactVector{}, point.radicand,
                     Sign::zero,           Dyadic(),      Dyadic()};
}

// A circle with centre c / W and squared radius rho / W^2, at the point
// P = (r + q sqrt(d)) / w. Then
//     P - c / W = (W r - w c + W q sqrt(d)) / (w W),
// and w W > 0, so V = (W r - w c) + (W q) sqrt(d) has the direction of
// P - centre. The tangent counterclockwise is V turned by a quarter turn
// counterclockwise; the curvature is positive counterclockwise (the circle
// turns left) and negative clockwise, of magnitude W / sqrt(rho).
Departure circleDeparture(const ExactPoint& point, const CircleSupport& circle, Heading heading) {
    assert(circle.weight.sign() == Sign::positive && "a circle needs a positive weight");
    assert(circle.radiusSquared.sign() == Sign::positive && "a circle needs a positive radius");
    const ExactVector centre{circle.centerX, circle.centerY};
    const ExactVector rational =
        minus(times(point.rational, circle.weight), times(centre, point.weight));
    const ExactVector radical = times(point.radical, circle.weight);
    ExactVector tangent = perpendicular(rational);
    ExactVector tangentRadical = perpendicular(radical);
    Sign bend = Sign::positive;
    if (heading == Heading::backward) {
        tangent = negated(tangent);
        tangentRadical = negated(tangentRadical);
        bend = Sign::negative;
    }
    return Departure{std::move(tangent),   std::move(tangentRadical),    point.radicand, bend,
                     circle.radiusSquared, circle.weight * circle.weight};
}

bool usesRadical(const Departure& departure) {
    return !(departure.tangentRadical.x.isZero() && departure.tangentRadical.y.isZero());
}

// True when the tangent lies in the half turn [0, pi) from +x: y > 0, or
// y == 0 and x > 0.
bool inFirstHalfTurn(const Departure& departure) {
    const Sign y =
        exact::signOf(departure.tangent.y, departure.tangentRadical.y, departure.radicand);
    if (y != Sign::zero) {
        return y == Sign::positive;
    }
    return exact::signOf(departure.tangent.x, departure.tangentRadical.x, departure.radicand) ==
           Sign::positive;
}

// The sign of the cross product of two tangents with the same radicand d:
//     (ax + bx s)(cy + ey s) - (ay + by s)(cx + ex s),   s = sqrt(d),
// whose rational part is ax cy - ay cx + (bx ey - by ex) d and whose radical
// part is ax ey + bx cy - ay ex - by cx.
Sign crossOfTangents(const Departure& u, const Departure& v, const Dyadic& radicand) {
    const auto& a = u.tangent;
    const auto& b = u.tangentRadical;
    const auto& c = v.tangent;
    const auto& e = v.tangentRadical;
    const Dyadic rational = a.x * c.y - a.y * c.x + (b.x * e.y - b.y * e.x) * radicand;
    const Dyadic radical = a.x * e.y + b.x * c.y - a.y * e.x - b.y * c.x;
    return exact::signOf(rational, radical, radicand);
}

// The sign of angle(u) - angle(v), angles in [0, 2 pi) from +x.
Sign compareTangents(const Departure& u, const Departure& v) {
    const bool firstU = inFirstHalfTurn(u);
    const bool firstV = inFirstHalfTurn(v);
    if (firstU != firstV) {
        return firstU ? Sign::negative : Sign::positive;
    }
    // In the same half turn the angles differ by less than pi: v comes after
    // u exactly when u x v is positive.
    const Dyadic& radicand = usesRadical(u) ? u.radicand : v.radicand;
    return negated(crossOfTangents(u, v, radicand));
}

// The sign of k(u) - k(v) for the signed curvatures.
Sign compareCurvatures(const Departure& u, const Departure& v) {
    if (u.bend != v.bend) {
        // negative < zero < positive.
        return static_cast<int>(u.bend) < static_cast<int>(v.bend) ? Sign::negative
                                                                   : Sign::positive;
    }
    if (u.bend == Sign::zero) {
        return Sign::zero;
    }
    // Same sign s: |k| = W / sqrt(rho), so |k(u)| - |k(v)| has the sign of
    // R(v)^2 - R(u)^2 = rho_v / W_v^2 - rho_u / W_u^2, that is of
    // rho_v W_u^2 - rho_u W_v^2; k(u) - k(v) is that times s.
    const Sign magnitude =
        compare(v.radiusSquared * u.weightSquared, u.radiusSquared * v.weightSquared);
    return u.bend == Sign::positive ? magnitude : negated(magnitude);
}

}  // namespace

Heading reversed(Heading heading) noexcept {
    return heading == Heading::forward ? Heading::backward : Heading::forward;
}

Departure departureOf(const ExactPoint& point, const HalfPiece& piece) {
    assert(onCarrier(piece.carrier, point) && "the point must lie on the carrier");
    if (const auto* line = std::get_if<LineMeaning>(&piece.carrier)) {
        return lineDeparture(point, *line, piece.heading);
    }
    return circleDeparture(point, std::get<CircleSupport>(piece.carrier), piece.heading);
}

Departure rayDeparture(const ExactPoint& point) {
    return Departure{ExactVector{Dyadic(1.0), Dyadic()},
                     ExactVector{},
                     point.radicand,
                     Sign::zero,
                     Dyadic(),
                     Dyadic()};
}

Sign compareDepartures(const Departure& a, const Departure& b) {
    assert((!usesRadical(a) || !usesRadical(b) || compare(a.radicand, b.radicand) == Sign::zero) &&
           "departures must come from the same point");
    const Sign tangents = compareTangents(a, b);
    if (tangents != Sign::zero) {
        return tangents;
    }
    return compareCurvatures(a, b);
}

Sign compareAround(const ExactPoint& point, const HalfPiece& a, const HalfPiece& b) {
    return compareDepartures(departureOf(point, a), departureOf(point, b));
}

LocalOrder::LocalOrder(const ExactPoint& point, std::span<const HalfPiece> pieces)
    : m_point(point) {
    m_departures.reserve(pieces.size());
    for (const auto& piece : pieces) {
        m_departures.push_back(departureOf(point, piece));
    }
    m_sorted.resize(pieces.size());
    std::iota(m_sorted.begin(), m_sorted.end(), std::size_t{0});
    // A strict weak order (a lexicographic order of exact keys), so the
    // stable sort is well defined and keeps the input order within a group.
    std::stable_sort(m_sorted.begin(), m_sorted.end(), [this](std::size_t a, std::size_t b) {
        return compareDepartures(m_departures[a], m_departures[b]) == Sign::negative;
    });
    m_groupOf.resize(pieces.size());
    for (std::size_t k = 0; k < m_sorted.size(); ++k) {
        if (k == 0 || compareDepartures(m_departures[m_sorted[k - 1]], m_departures[m_sorted[k]]) !=
                          Sign::zero) {
            m_groupStart.push_back(k);
        }
        m_groupOf[m_sorted[k]] = m_groupStart.size() - 1;
    }
}

std::span<const std::size_t> LocalOrder::group(std::size_t g) const {
    assert(g < m_groupStart.size());
    const std::size_t first = m_groupStart[g];
    const std::size_t last = g + 1 < m_groupStart.size() ? m_groupStart[g + 1] : m_sorted.size();
    return std::span<const std::size_t>(m_sorted).subspan(first, last - first);
}

std::size_t LocalOrder::groupOf(std::size_t i) const {
    assert(i < m_groupOf.size());
    return m_groupOf[i];
}

const Departure& LocalOrder::departure(std::size_t i) const {
    assert(i < m_departures.size());
    return m_departures[i];
}

Placement LocalOrder::place(const HalfPiece& reference) const {
    return place(departureOf(m_point, reference));
}

Placement LocalOrder::place(const Departure& reference) const {
    const std::size_t count = m_groupStart.size();
    if (count == 0) {
        return Placement{false, 0};
    }
    // The first group not before the reference.
    std::size_t low = 0;
    std::size_t high = count;
    while (low < high) {
        const std::size_t middle = low + (high - low) / 2;
        const auto& representative = m_departures[m_sorted[m_groupStart[middle]]];
        if (compareDepartures(representative, reference) == Sign::negative) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    if (low < count &&
        compareDepartures(m_departures[m_sorted[m_groupStart[low]]], reference) == Sign::zero) {
        return Placement{true, low};
    }
    // Between group low - 1 and group low; before group 0 it is the sector
    // that closes the turn, after the last group.
    return Placement{false, low == 0 ? count - 1 : low - 1};
}

}  // namespace solid_space::geom2d::detail
