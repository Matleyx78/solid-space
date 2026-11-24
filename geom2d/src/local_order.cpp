// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// The exact order of the pieces that leave a point (local_order.hpp; stage
// 0.3, package PS2).
//
// Floating-point filter of compareDepartures() (stage 0.3 corrections,
// package FS2; exact/interval.hpp). The order of two departures reads a few
// signs: the half turn of each tangent (the sign of its y, and of its x when y
// is zero), the cross product of the two tangents when they lie in the same
// half turn, and the curvatures when the tangents are equal. With the filter,
// the signs of the tangents are first read from intervals: each coordinate
// t + s sqrt(d) is one sum and one product of intervals of the Dyadic data
// (with the interval of the square root of the exact radicand), so it contains
// the exact coordinate, and the cross product is two products and a
// difference of those. A sign whose interval contains zero is computed by its
// exact formula, the same function as on the exact path (inFirstHalfTurn(),
// crossOfTangents()), and the curvatures are always compared exactly
// (compareCurvatures()). The decisions that follow are those of
// compareTangents() and exactCompareDepartures() on the same signs, so the
// answer is the same; the intervals never answer zero. A LocalOrder computes
// the intervals and the half turn of each departure once, for the whole sort.

#include "local_order.hpp"

#include "exact/interval.hpp"
#include "exact/quadratic.hpp"

#include <algorithm>
#include <cassert>
#include <numeric>
#include <optional>
#include <utility>
#include <variant>

namespace solid_space::geom2d::detail {
namespace {

using exact::Interval;

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

Sign exactCompareDepartures(const Departure& a, const Departure& b) {
    assert((!usesRadical(a) || !usesRadical(b) || compare(a.radicand, b.radicand) == Sign::zero) &&
           "departures must come from the same point");
    const Sign tangents = compareTangents(a, b);
    if (tangents != Sign::zero) {
        return tangents;
    }
    return compareCurvatures(a, b);
}

// --- Filter ------------------------------------------------------------------

// What the filter reads of a departure: the intervals of its tangent, and its
// half turn (inFirstHalfTurn()), from the intervals when they decide it and
// otherwise exactly; exactHalfTurn says which.
struct TangentIntervals {
    Interval x;
    Interval y;
    bool firstHalfTurn = false;
    bool exactHalfTurn = false;
};

// t + s * root, or t alone without a radical.
Interval coordinateInterval(const Dyadic& rational, const Dyadic& radical, const Interval* root) {
    Interval value = exact::intervalOf(rational);
    if (root != nullptr && !radical.isZero()) {
        value = value + (exact::intervalOf(radical) * *root);
    }
    return value;
}

// inFirstHalfTurn() on the signs of the intervals: y > 0, or y == 0 and x > 0.
std::optional<bool> filteredHalfTurn(const Departure& departure, const Interval& x,
                                     const Interval& y) {
    if (const auto sign = exact::certainSign(y)) {
        return *sign == Sign::positive;
    }
    // y is exactly zero when both of its Dyadic parts are (or the radicand
    // is): an exact fact, not a decision of the filter.
    const bool yZero = departure.tangent.y.isZero() &&
                       (departure.tangentRadical.y.isZero() || departure.radicand.isZero());
    if (yZero) {
        if (const auto sign = exact::certainSign(x)) {
            return *sign == Sign::positive;
        }
    }
    return std::nullopt;
}

TangentIntervals tangentIntervals(const Departure& departure) {
    Interval root;
    const bool radical = usesRadical(departure) && !departure.radicand.isZero();
    if (radical) {
        // sqrt(d) of the exact radicand d >= 0 (the radicand of a point).
        root = exact::squareRoot(exact::intervalOf(departure.radicand));
    }
    const Interval* rootOrNull = radical ? &root : nullptr;
    TangentIntervals result;
    result.x = coordinateInterval(departure.tangent.x, departure.tangentRadical.x, rootOrNull);
    result.y = coordinateInterval(departure.tangent.y, departure.tangentRadical.y, rootOrNull);
    if (const auto half = filteredHalfTurn(departure, result.x, result.y)) {
        result.firstHalfTurn = *half;
    } else {
        result.firstHalfTurn = inFirstHalfTurn(departure);
        result.exactHalfTurn = true;
    }
    return result;
}

// compareTangents() with its signs filtered: the half turns as
// tangentIntervals() found them, and the cross product of the two tangents
// (two products and a difference of intervals), exactly by crossOfTangents()
// when its interval contains zero. At equal tangents the curvatures decide,
// exactly (compareCurvatures(), no floating point). The same decisions on the
// same signs as exactCompareDepartures().
Sign mixedCompare(const Departure& a, const TangentIntervals& u, const Departure& b,
                  const TangentIntervals& v) {
    bool exactUsed = u.exactHalfTurn || v.exactHalfTurn;
    Sign tangents = Sign::zero;
    if (u.firstHalfTurn != v.firstHalfTurn) {
        tangents = u.firstHalfTurn ? Sign::negative : Sign::positive;
    } else if (const auto cross = exact::certainSign((u.x * v.y) - (u.y * v.x))) {
        tangents = negated(*cross);
    } else {
        exactUsed = true;
        const Dyadic& radicand = usesRadical(a) ? a.radicand : b.radicand;
        tangents = negated(crossOfTangents(a, b, radicand));
    }
    if (tangents == Sign::zero) {
        exactUsed = true;
        tangents = compareCurvatures(a, b);
    }
    exact::recordFilterAnswer(!exactUsed);
    return tangents;
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
    if (exact::filterEnabled()) {
        assert(
            (!usesRadical(a) || !usesRadical(b) || compare(a.radicand, b.radicand) == Sign::zero) &&
            "departures must come from the same point");
        return mixedCompare(a, tangentIntervals(a), b, tangentIntervals(b));
    }
    return exactCompareDepartures(a, b);
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
    // The intervals of every departure, once for the whole sort.
    std::vector<TangentIntervals> intervals;
    const bool filtered = exact::filterEnabled();
    if (filtered) {
        intervals.reserve(m_departures.size());
        for (const auto& departure : m_departures) {
            intervals.push_back(tangentIntervals(departure));
        }
    }
    const auto order = [&](std::size_t a, std::size_t b) {
        return filtered ? mixedCompare(m_departures[a], intervals[a], m_departures[b], intervals[b])
                        : exactCompareDepartures(m_departures[a], m_departures[b]);
    };
    m_sorted.resize(pieces.size());
    std::iota(m_sorted.begin(), m_sorted.end(), std::size_t{0});
    // A strict weak order (a lexicographic order of exact keys), so the
    // stable sort is well defined and keeps the input order within a group.
    std::stable_sort(m_sorted.begin(), m_sorted.end(), [&order](std::size_t a, std::size_t b) {
        return order(a, b) == Sign::negative;
    });
    m_groupOf.resize(pieces.size());
    for (std::size_t k = 0; k < m_sorted.size(); ++k) {
        if (k == 0 || order(m_sorted[k - 1], m_sorted[k]) != Sign::zero) {
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
    // The intervals of the reference, once for the whole search.
    std::optional<TangentIntervals> referenceIntervals;
    if (exact::filterEnabled()) {
        referenceIntervals = tangentIntervals(reference);
    }
    const auto order = [&](const Departure& representative) {
        if (referenceIntervals) {
            return mixedCompare(representative, tangentIntervals(representative), reference,
                                *referenceIntervals);
        }
        return exactCompareDepartures(representative, reference);
    };
    // The first group not before the reference.
    std::size_t low = 0;
    std::size_t high = count;
    while (low < high) {
        const std::size_t middle = low + (high - low) / 2;
        const auto& representative = m_departures[m_sorted[m_groupStart[middle]]];
        if (order(representative) == Sign::negative) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    if (low < count && order(m_departures[m_sorted[m_groupStart[low]]]) == Sign::zero) {
        return Placement{true, low};
    }
    // Between group low - 1 and group low; before group 0 it is the sector
    // that closes the turn, after the last group.
    return Placement{false, low == 0 ? count - 1 : low - 1};
}

}  // namespace solid_space::geom2d::detail
