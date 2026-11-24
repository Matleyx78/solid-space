// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// The parity of a point and of the sectors around it with respect to closed
// loops (winding.hpp; stage 0.3, package PS2; boxes and bands, package FS1 of
// the 0.3 performance work).
//
// Why skipping a box is exact. A piece is skipped only when its answer is
// certain, from three facts:
// 1. the box of a piece contains every point of it exactly (pieceBox(): the
//    ends of a polyline piece are doubles of the data, and the box of a
//    circle is the one of arrangement.cpp, whose margins cover the rounding
//    of its centre and radius);
// 2. the corners of a box are doubles;
// 3. nearestPoint() (positions.hpp) rounds every coordinate correctly and is
//    a non-decreasing function of the exact value, and a double rounds to
//    itself.
// So for an exact coordinate v, its rounding r and a corner c: v >= c gives
// r >= c, and v <= c gives r <= c. Read backwards, r < c proves v < c and
// r > c proves v > c, whatever the rounding error, near zero (subnormal
// roundings are monotone too) and at the limits of the domain (the
// coordinates stay below 2^1000, the precondition of nearestPoint()). No
// margin on the point is needed: the margins are all in the boxes. Then, for
// a point P rounded to p and a piece in the box B:
// - P lies on the piece only if p lies in B (closed), since P lies in B;
// - a part of the piece is crossed by the half-line of P (winding.hpp, "The
//   method") only if p.y lies in [B.minY, B.maxY] and p.x <= B.maxX. If
//   p.y < B.minY, every point of the piece is strictly above P, so no part
//   has exactly one end strictly above it; if p.y > B.maxY, no point of the
//   piece is above P. If p.x > B.maxX, a point of the piece at the height of
//   P is strictly left of P, so it is not right of P.
// A piece that the box does not exclude is looked at as in stage 0.3.

#include "winding.hpp"

#include "positions.hpp"
#include "solid_space/geom2d/curves.hpp"
#include "solid_space/geom2d/intersection.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <numeric>
#include <optional>
#include <utility>
#include <variant>

namespace solid_space::geom2d::detail {
namespace {

constexpr double kInfinity = std::numeric_limits<double>::infinity();

// The directions from a centre: straight up, straight down, right and left.
ExactVector up() {
    return {Dyadic(0.0), Dyadic(1.0)};
}

ExactVector down() {
    return {Dyadic(0.0), Dyadic(-1.0)};
}

ExactVector rightward() {
    return {Dyadic(1.0), Dyadic(0.0)};
}

ExactVector leftward() {
    return {Dyadic(-1.0), Dyadic(0.0)};
}

// The half of the circle a part lies on, from the direction (from the
// centre) where it starts and the turn it runs in. Off the vertical, the sign
// of x decides; from the top, counterclockwise goes left and clockwise
// right; from the bottom, the other way round.
CircleHalf halfFrom(const ExactVector& start, Turn turn) {
    switch (start.x.sign()) {
        case Sign::positive:
            return CircleHalf::right;
        case Sign::negative:
            return CircleHalf::left;
        case Sign::zero:
            break;
    }
    const bool top = start.y.sign() == Sign::positive;
    const bool counterclockwise = turn == Turn::counterclockwise;
    return top == counterclockwise ? CircleHalf::left : CircleHalf::right;
}

// The exact ends of a polyline piece are its doubles; every other curve uses
// positions.hpp.
ExactPoint startOf(const CurveMeaning& meaning) {
    if (const auto* piece = std::get_if<PolylineSegment>(&meaning.curve)) {
        return exactPoint(piece->start);
    }
    return startPoint(meaning);
}

ExactPoint endOf(const CurveMeaning& meaning) {
    if (const auto* piece = std::get_if<PolylineSegment>(&meaning.curve)) {
        return exactPoint(piece->end);
    }
    return endPoint(meaning);
}

// The parts of an arc that is not a whole circle: split where its direction
// from the centre is straight up or straight down, strictly inside the arc.
std::vector<MonotonePart> arcParts(const CurveMeaning& meaning, const ArcMeaning& arc) {
    struct Split {
        ExactVector direction;
        ExactPoint point;
    };

    std::vector<Split> splits;
    for (auto direction : {up(), down()}) {
        if (locateDirection(arc, direction) == Location::inside) {
            auto point = pointInDirection(arc.circle, direction);
            splits.push_back({std::move(direction), std::move(point)});
        }
    }
    if (splits.size() == 2 &&
        compareAlongArc(arc, splits[1].direction, splits[0].direction) == Sign::negative) {
        std::swap(splits[0], splits[1]);
    }
    std::vector<MonotonePart> parts;
    ExactPoint from = startOf(meaning);
    ExactVector fromDirection = arc.startDirection;
    for (auto& split : splits) {
        parts.push_back({std::move(from), split.point, halfFrom(fromDirection, arc.turn)});
        from = std::move(split.point);
        fromDirection = split.direction;
    }
    parts.push_back({std::move(from), endOf(meaning), halfFrom(fromDirection, arc.turn)});
    return parts;
}

std::vector<MonotonePart> partsOf(const CurveMeaning& meaning) {
    if (std::holds_alternative<LineMeaning>(meaning.support)) {
        return {MonotonePart{startOf(meaning), endOf(meaning), CircleHalf::right}};
    }
    const auto& arc = std::get<ArcMeaning>(meaning.support);
    if (arc.span == ArcSpan::circle || arc.span == ArcSpan::whole) {
        auto bottom = pointInDirection(arc.circle, down());
        auto top = pointInDirection(arc.circle, up());
        return {MonotonePart{bottom, top, CircleHalf::right},
                MonotonePart{std::move(top), std::move(bottom), CircleHalf::left}};
    }
    return arcParts(meaning, arc);
}

bool above(const ExactPoint& point, const ExactPoint& reference) {
    return compareY(point, reference) == Sign::positive;
}

// True when the part crosses the half-line y = Py + e, x > Px, for an
// infinitely small e > 0, away from P (winding.hpp, "The method").
bool crossesRightOf(const ExactPoint& point, const MonotonePart& part, const Carrier& carrier) {
    const bool upward = above(part.to, point);
    if (above(part.from, point) == upward) {
        return false;
    }
    if (const auto* line = std::get_if<LineMeaning>(&carrier)) {
        // Going up, the points left of the line are those before its point
        // at height Py; going down, those after it. A zero: the part passes
        // through P.
        const Sign side = lineSign(*line, point);
        return side == (upward ? Sign::positive : Sign::negative);
    }
    // The point of the part at height Py is Cx + sqrt(Q) on the right half,
    // Cx - sqrt(Q) on the left one, with Q = R^2 - (Py - Cy)^2 >= 0 since the
    // part reaches that height. On the right: Cx > Px gives a point right of
    // P; otherwise sqrt(Q) - (Px - Cx) has the sign of Q - (Px - Cx)^2, that
    // is R^2 - |P - C|^2, positive when P is inside the circle. On the left:
    // Cx <= Px gives a point not right of P; otherwise (Cx - Px) - sqrt(Q) has
    // the sign of |P - C|^2 - R^2, positive when P is outside. A zero means
    // the point at height Py is P itself.
    const auto& circle = std::get<CircleSupport>(carrier);
    const ExactPoint centre{ExactVector{circle.centerX, circle.centerY}, ExactVector{}, Dyadic(),
                            circle.weight};
    const Sign centreRight = compareX(centre, point);
    const Sign inside = circleSign(circle, point);
    if (part.half == CircleHalf::right) {
        return centreRight == Sign::positive || inside == Sign::negative;
    }
    return centreRight == Sign::positive && inside == Sign::positive;
}

bool onPiece(const LoopPiece& piece, const ExactPoint& point) {
    if (!onCarrier(piece.carrier, point)) {
        return false;
    }
    const Location location = locate(piece.meaning, point);
    return location == Location::atStart || location == Location::inside ||
           location == Location::atEnd;
}

// The heading of the carrier along the direction of the piece.
Heading headingAlong(const CurveMeaning& meaning) {
    if (const auto* arc = std::get_if<ArcMeaning>(&meaning.support)) {
        return arc->turn == Turn::counterclockwise ? Heading::forward : Heading::backward;
    }
    return Heading::forward;
}

// The box of two points of doubles: exact, with no margin.
Box boxOfEnds(Point2 a, Point2 b) {
    return Box{std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x), std::max(a.y, b.y)};
}

bool contains(const Box& box, Point2 point) {
    return box.minX <= point.x && point.x <= box.maxX && box.minY <= point.y && point.y <= box.maxY;
}

// The pieces a point looks at, by slot and in increasing order: with boxes,
// those whose box meets the half-line of the rounded point toward +x, the
// point included (see "Why skipping a box is exact"); with everyPiece, all of
// them, and the point is not rounded.
struct Nearby {
    std::vector<std::size_t> slots;
    std::optional<Point2> rounded;
};

Nearby nearby(const ExactPoint& point, const LoopIndex& loops) {
    if (loops.search() == PieceSearch::everyPiece) {
        return {loops.meeting(Box{}), std::nullopt};
    }
    const Point2 rounded = nearestPoint(point);
    return {loops.meeting(Box{rounded.x, rounded.y, kInfinity, rounded.y}), rounded};
}

// False only when the point is certain not to lie on a piece in the box.
bool mayLieIn(const Nearby& near, const Box& box) {
    return !near.rounded.has_value() || contains(box, *near.rounded);
}

bool parityOf(const ExactPoint& point, const LoopIndex& loops, const Nearby& near) {
    bool odd = false;
    for (const std::size_t slot : near.slots) {
        const LoopPiece& piece = loops.piece(slot);
        for (const auto& part : piece.parts) {
            if (crossesRightOf(point, part, piece.carrier)) {
                odd = !odd;
            }
        }
    }
    return odd;
}

}  // namespace

Box pieceBox(const CurveMeaning& meaning, const Carrier& carrier) {
    assert(!meaning.degenerate && "a piece of a loop is not a point");
    if (const auto* line = std::get_if<LineMeaning>(&carrier)) {
        // The ends of a straight curve are its doubles (curve_meaning.hpp).
        return boxOfEnds(line->start, line->end);
    }
    ArrangementCurve whole;
    whole.meaning = meaning;
    const Box circle = boxOf(whole);
    const auto& arc = std::get<ArcMeaning>(meaning.support);
    const auto* piece = std::get_if<PolylineSegment>(&meaning.curve);
    if (piece == nullptr || arc.span != ArcSpan::partial) {
        return circle;
    }
    // Along an arc, x is largest at its rightmost point if the arc has it
    // strictly inside, and otherwise at one of its ends: x has a single
    // maximum on the circle, in direction (1, 0) from the centre. The same for
    // the other three sides. The ends of a polyline piece are its doubles.
    Box box = boxOfEnds(piece->start, piece->end);
    if (locateDirection(arc, rightward()) == Location::inside) {
        box.maxX = circle.maxX;
    }
    if (locateDirection(arc, leftward()) == Location::inside) {
        box.minX = circle.minX;
    }
    if (locateDirection(arc, up()) == Location::inside) {
        box.maxY = circle.maxY;
    }
    if (locateDirection(arc, down()) == Location::inside) {
        box.minY = circle.minY;
    }
    return box;
}

LoopPiece loopPiece(std::size_t index, CurveMeaning meaning, Carrier carrier) {
    const Box box = pieceBox(meaning, carrier);
    auto parts = partsOf(meaning);
    return LoopPiece{index, std::move(meaning), std::move(carrier), box, std::move(parts)};
}

IntervalIndex::IntervalIndex(std::span<const double> lows, std::span<const double> highs) {
    assert(lows.size() == highs.size() && "an interval needs both ends");
    const std::size_t count = lows.size();
    m_items.resize(count);
    std::iota(m_items.begin(), m_items.end(), std::size_t{0});
    std::stable_sort(m_items.begin(), m_items.end(),
                     [&lows](std::size_t a, std::size_t b) { return lows[a] < lows[b]; });
    m_lows.reserve(count);
    for (const std::size_t item : m_items) {
        assert(!std::isnan(lows[item]) && !std::isnan(highs[item]) && "no NaN");
        m_lows.push_back(lows[item]);
    }
    m_leaves = std::bit_ceil(std::max(count, std::size_t{1}));
    m_largestHigh.assign(2 * m_leaves, -kInfinity);
    for (std::size_t slot = 0; slot < count; ++slot) {
        m_largestHigh[m_leaves + slot] = highs[m_items[slot]];
    }
    for (std::size_t node = m_leaves - 1; node >= 1; --node) {
        m_largestHigh[node] = std::max(m_largestHigh[2 * node], m_largestHigh[(2 * node) + 1]);
    }
}

std::vector<std::size_t> IntervalIndex::meeting(double low, double high) const {
    assert(!std::isnan(low) && !std::isnan(high) && "no NaN");
    std::vector<std::size_t> found;
    // The slots before `end` have a low end <= high; among them, a node whose
    // largest high end is below `low` has no interval that meets the query.
    const auto end = static_cast<std::size_t>(std::upper_bound(m_lows.begin(), m_lows.end(), high) -
                                              m_lows.begin());

    struct Node {
        std::size_t node;
        std::size_t first;
        std::size_t count;
    };

    std::vector<Node> pending;
    if (end > 0) {
        pending.push_back({1, 0, m_leaves});
    }
    while (!pending.empty()) {
        const Node visit = pending.back();
        pending.pop_back();
        if (visit.first >= end || m_largestHigh[visit.node] < low) {
            continue;
        }
        if (visit.count == 1) {
            found.push_back(m_items[visit.first]);
            continue;
        }
        const std::size_t half = visit.count / 2;
        pending.push_back({(2 * visit.node) + 1, visit.first + half, half});
        pending.push_back({2 * visit.node, visit.first, half});
    }
    std::sort(found.begin(), found.end());
    return found;
}

LoopIndex::LoopIndex(std::span<const LoopPieces> loops, PieceSearch search)
    : m_loops(loops), m_search(search), m_whole{kInfinity, kInfinity, -kInfinity, -kInfinity} {
    for (std::size_t l = 0; l < loops.size(); ++l) {
        for (std::size_t position = 0; position < loops[l].pieces.size(); ++position) {
            m_slots.push_back(PieceSlot{l, position});
            const Box& box = loops[l].pieces[position].box;
            m_whole.minX = std::min(m_whole.minX, box.minX);
            m_whole.minY = std::min(m_whole.minY, box.minY);
            m_whole.maxX = std::max(m_whole.maxX, box.maxX);
            m_whole.maxY = std::max(m_whole.maxY, box.maxY);
        }
    }
    if (m_search == PieceSearch::automatic) {
        m_search = m_slots.size() > kFewPieces ? PieceSearch::boxes : PieceSearch::everyPiece;
    }
    if (m_search == PieceSearch::boxes) {
        std::vector<double> lows;
        std::vector<double> highs;
        lows.reserve(m_slots.size());
        highs.reserve(m_slots.size());
        for (std::size_t number = 0; number < m_slots.size(); ++number) {
            lows.push_back(piece(number).box.minY);
            highs.push_back(piece(number).box.maxY);
        }
        m_bands = IntervalIndex(lows, highs);
    }
}

const LoopPiece& LoopIndex::piece(std::size_t number) const {
    const PieceSlot& place = m_slots.at(number);
    return m_loops[place.loop].pieces[place.position];
}

std::vector<std::size_t> LoopIndex::meeting(const Box& box) const {
    if (m_search == PieceSearch::everyPiece) {
        std::vector<std::size_t> all(m_slots.size());
        std::iota(all.begin(), all.end(), std::size_t{0});
        return all;
    }
    auto found = m_bands.meeting(box.minY, box.maxY);
    std::erase_if(found, [this, &box](std::size_t number) {
        const Box& other = piece(number).box;
        return other.maxX < box.minX || box.maxX < other.minX;
    });
    return found;
}

LoopPieces loopPieces(const Loop& loop) {
    LoopPieces result;
    auto add = [&result](std::size_t index, const Curve& curve) {
        CurveMeaning meaning = meaningOf(curve);
        if (meaning.degenerate) {
            return;
        }
        Carrier carrier = carrierOf(meaning);
        result.pieces.push_back(loopPiece(index, std::move(meaning), std::move(carrier)));
    };
    if (const auto* circle = std::get_if<Circle>(&loop)) {
        add(0, Curve{*circle});
    } else {
        // A loop is closed whatever its flag says (region.hpp).
        Polyline closed = std::get<Polyline>(loop);
        closed.closed = true;
        const auto segments = pieces(closed);
        for (std::size_t i = 0; i < segments.size(); ++i) {
            add(i, Curve{segments[i]});
        }
    }
    assert(!result.pieces.empty() && "a loop needs a piece that is not a point");
    return result;
}

PointStatus pointStatus(const ExactPoint& point, const LoopIndex& loops) {
    PointStatus status;
    const Nearby near = nearby(point, loops);
    for (const std::size_t slot : near.slots) {
        const LoopPiece& piece = loops.piece(slot);
        if (mayLieIn(near, piece.box) && onPiece(piece, point)) {
            status.pieces.push_back(PieceRef{loops.slot(slot).loop, piece.index});
        }
    }
    if (!status.pieces.empty()) {
        status.state = PointState::on;
    } else {
        status.state = parityOf(point, loops, near) ? PointState::inside : PointState::outside;
    }
    return status;
}

PointStatus pointStatus(const ExactPoint& point, std::span<const LoopPieces> loops) {
    return pointStatus(point, LoopIndex(loops));
}

bool parityAboveRay(const ExactPoint& point, const LoopIndex& loops) {
    return parityOf(point, loops, nearby(point, loops));
}

bool parityAboveRay(const ExactPoint& point, std::span<const LoopPieces> loops) {
    return parityAboveRay(point, LoopIndex(loops));
}

SectorStates sectorStates(const ExactPoint& point, std::span<const LoopPieces> loops,
                          std::span<const HalfPiece> references) {
    return sectorStates(point, LoopIndex(loops), references);
}

SectorStates sectorStates(const ExactPoint& point, const LoopIndex& loops,
                          std::span<const HalfPiece> references) {
    SectorStates states;
    const Nearby near = nearby(point, loops);
    for (const std::size_t slot : near.slots) {
        const LoopPiece& piece = loops.piece(slot);
        if (!mayLieIn(near, piece.box) || !onCarrier(piece.carrier, point)) {
            continue;
        }
        const Location location = locate(piece.meaning, point);
        const Heading along = headingAlong(piece.meaning);
        const PieceRef owner{loops.slot(slot).loop, piece.index};
        if (location == Location::atStart || location == Location::inside) {
            states.halfPieces.push_back(HalfPiece{piece.carrier, along});
            states.owners.push_back(owner);
        }
        if (location == Location::atEnd || location == Location::inside) {
            states.halfPieces.push_back(HalfPiece{piece.carrier, reversed(along)});
            states.owners.push_back(owner);
        }
    }
    // Every vertex of a closed loop is the end of one piece and the start of
    // the next, and a point inside a piece gives two half pieces.
    assert(states.owners.size() % 2 == 0 && "the loops must be closed");
    states.halfPieces.insert(states.halfPieces.end(), references.begin(), references.end());
    states.order = LocalOrder(point, states.halfPieces);

    const bool parity = parityOf(point, loops, near);
    const std::size_t count = states.order.groupCount();
    if (count == 0) {
        states.inside.push_back(parity);
        return states;
    }
    states.inside.assign(count, false);
    const std::size_t first = states.order.place(rayDeparture(point)).index;
    states.inside[first] = parity;
    const std::size_t loopHalves = states.owners.size();
    for (std::size_t step = 1; step < count; ++step) {
        const std::size_t sector = (first + step) % count;
        const std::size_t previous = (sector + count - 1) % count;
        // Passing from the previous sector into this one crosses group
        // `sector`, which flips the parity once for each loop half piece.
        const auto members = states.order.group(sector);
        const auto flips = std::count_if(members.begin(), members.end(),
                                         [loopHalves](std::size_t i) { return i < loopHalves; });
        states.inside[sector] = (states.inside[previous] != (flips % 2 == 1));
    }
    return states;
}

}  // namespace solid_space::geom2d::detail
