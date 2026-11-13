// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// The parity of a point and of the sectors around it with respect to closed
// loops (winding.hpp; stage 0.3, package PS2).

#include "winding.hpp"

#include "positions.hpp"
#include "solid_space/geom2d/curves.hpp"
#include "solid_space/geom2d/intersection.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <variant>

namespace solid_space::geom2d::detail {
namespace {

// The side of its centre on which a part of a circle lies.
enum class CircleHalf : std::uint8_t {
    right,  // x >= centre x
    left,   // x <= centre x
};

// A part of a piece that is monotone in y, from one end to the other. For a
// part of a circle, half says on which side of the centre it lies; it is not
// read for a segment.
struct MonotonePart {
    ExactPoint from;
    ExactPoint to;
    CircleHalf half = CircleHalf::right;
};

// The directions straight up and straight down from a centre.
ExactVector up() {
    return {Dyadic(0.0), Dyadic(1.0)};
}

ExactVector down() {
    return {Dyadic(0.0), Dyadic(-1.0)};
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

}  // namespace

LoopPieces loopPieces(const Loop& loop) {
    LoopPieces result;
    auto add = [&result](std::size_t index, const Curve& curve) {
        CurveMeaning meaning = meaningOf(curve);
        if (meaning.degenerate) {
            return;
        }
        Carrier carrier = carrierOf(meaning);
        result.pieces.push_back(LoopPiece{index, std::move(meaning), std::move(carrier)});
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

PointStatus pointStatus(const ExactPoint& point, std::span<const LoopPieces> loops) {
    PointStatus status;
    for (std::size_t l = 0; l < loops.size(); ++l) {
        for (const auto& piece : loops[l].pieces) {
            if (onPiece(piece, point)) {
                status.pieces.push_back(PieceRef{l, piece.index});
            }
        }
    }
    if (!status.pieces.empty()) {
        status.state = PointState::on;
    } else {
        status.state = parityAboveRay(point, loops) ? PointState::inside : PointState::outside;
    }
    return status;
}

bool parityAboveRay(const ExactPoint& point, std::span<const LoopPieces> loops) {
    bool odd = false;
    for (const auto& loop : loops) {
        for (const auto& piece : loop.pieces) {
            for (const auto& part : partsOf(piece.meaning)) {
                if (crossesRightOf(point, part, piece.carrier)) {
                    odd = !odd;
                }
            }
        }
    }
    return odd;
}

SectorStates sectorStates(const ExactPoint& point, std::span<const LoopPieces> loops,
                          std::span<const HalfPiece> references) {
    SectorStates states;
    for (std::size_t l = 0; l < loops.size(); ++l) {
        for (const auto& piece : loops[l].pieces) {
            if (!onCarrier(piece.carrier, point)) {
                continue;
            }
            const Location location = locate(piece.meaning, point);
            const Heading along = headingAlong(piece.meaning);
            const PieceRef owner{l, piece.index};
            if (location == Location::atStart || location == Location::inside) {
                states.halfPieces.push_back(HalfPiece{piece.carrier, along});
                states.owners.push_back(owner);
            }
            if (location == Location::atEnd || location == Location::inside) {
                states.halfPieces.push_back(HalfPiece{piece.carrier, reversed(along)});
                states.owners.push_back(owner);
            }
        }
    }
    // Every vertex of a closed loop is the end of one piece and the start of
    // the next, and a point inside a piece gives two half pieces.
    assert(states.owners.size() % 2 == 0 && "the loops must be closed");
    states.halfPieces.insert(states.halfPieces.end(), references.begin(), references.end());
    states.order = LocalOrder(point, states.halfPieces);

    const bool parity = parityAboveRay(point, loops);
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
