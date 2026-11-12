// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Unit tests of the parity of points and sectors with respect to loops (stage
// 0.3, package PS2). The loops are rectangles, circles and arcs with small
// integer data, so every expected state can be seen by hand; the points a
// rounding away from a side or an arc are the neighbours of points written
// exactly on it.

#include "winding.hpp"

#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "local_order.hpp"
#include "positions.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <span>
#include <variant>
#include <vector>

namespace {

using solid_space::geom2d::Arc;
using solid_space::geom2d::Circle;
using solid_space::geom2d::Curve;
using solid_space::geom2d::Loop;
using solid_space::geom2d::Point2;
using solid_space::geom2d::Polyline;
using solid_space::geom2d::PolylineVertex;
using solid_space::geom2d::Segment;
using solid_space::geom2d::detail::ExactPoint;
using solid_space::geom2d::detail::HalfPiece;
using solid_space::geom2d::detail::Heading;
using solid_space::geom2d::detail::LoopPieces;
using solid_space::geom2d::detail::PieceRef;
using solid_space::geom2d::detail::PointState;
using solid_space::robust::Sign;

namespace detail = solid_space::geom2d::detail;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

double below(double value) {
    return std::nextafter(value, -kInfinity);
}

double above(double value) {
    return std::nextafter(value, kInfinity);
}

ExactPoint at(double x, double y) {
    return detail::exactPoint(Point2{x, y});
}

// A closed polyline from its vertices (position and bulge).
Loop polygon(std::initializer_list<PolylineVertex> vertices) {
    return Polyline{vertices, true};
}

Loop rectangle(double x0, double y0, double x1, double y1) {
    return polygon({{{x0, y0}, 0.0}, {{x1, y0}, 0.0}, {{x1, y1}, 0.0}, {{x0, y1}, 0.0}});
}

std::vector<LoopPieces> describe(std::span<const Loop> loops) {
    std::vector<LoopPieces> result;
    for (const auto& loop : loops) {
        result.push_back(detail::loopPieces(loop));
    }
    return result;
}

PointState stateOf(const ExactPoint& point, std::span<const Loop> loops) {
    return detail::pointStatus(point, describe(loops)).state;
}

PointState stateOf(double x, double y, std::span<const Loop> loops) {
    return stateOf(at(x, y), loops);
}

std::vector<PieceRef> piecesAt(double x, double y, std::span<const Loop> loops) {
    return detail::pointStatus(at(x, y), describe(loops)).pieces;
}

std::vector<bool> sectorsOf(const ExactPoint& point, std::span<const Loop> loops,
                            std::span<const HalfPiece> references = {}) {
    return detail::sectorStates(point, describe(loops), references).inside;
}

ExactPoint meetingWithXAbove(const Curve& first, const Curve& second, double x) {
    const auto meetFirst = detail::carrierOf(detail::meaningOf(first));
    const auto meetSecond = detail::carrierOf(detail::meaningOf(second));
    const auto meeting = detail::meet(meetFirst, meetSecond);
    for (const auto& point : meeting.found()) {
        if (detail::compareX(point.point, at(x, 0.0)) == Sign::positive) {
            return point.point;
        }
    }
    ADD_FAILURE() << "no meeting right of " << x;
    return at(0.0, 0.0);
}

ExactPoint meetingWithXBelow(const Curve& first, const Curve& second, double x) {
    const auto meeting = detail::meet(detail::carrierOf(detail::meaningOf(first)),
                                      detail::carrierOf(detail::meaningOf(second)));
    for (const auto& point : meeting.found()) {
        if (detail::compareX(point.point, at(x, 0.0)) == Sign::negative) {
            return point.point;
        }
    }
    ADD_FAILURE() << "no meeting left of " << x;
    return at(0.0, 0.0);
}

HalfPiece half(const Curve& curve, Heading heading) {
    return HalfPiece{detail::carrierOf(detail::meaningOf(curve)), heading};
}

constexpr PointState kIn = PointState::inside;
constexpr PointState kOut = PointState::outside;
constexpr PointState kOn = PointState::on;

// --- The loops of the tests ---------------------------------------------

// Two semicircles joined by two segments: (0, 0) to (4, 0), the right
// semicircle of centre (4, 1) and radius 1 counterclockwise up to (4, 2),
// back to (0, 2), the left semicircle of centre (0, 1) counterclockwise down
// to (0, 0).
Loop stadium() {
    return polygon({{{0.0, 0.0}, 0.0}, {{4.0, 0.0}, 1.0}, {{4.0, 2.0}, 0.0}, {{0.0, 2.0}, 1.0}});
}

// The lower half of the disc of centre (2, 0) and radius 2: the arc from
// (0, 0) to (4, 0) counterclockwise (bulge 1) through (2, -2), and the
// segment back.
Loop lowerHalfDisc() {
    return polygon({{{0.0, 0.0}, 1.0}, {{4.0, 0.0}, 0.0}});
}

// A bow tie that crosses itself at (1, 1).
Loop bowTie() {
    return polygon({{{0.0, 0.0}, 0.0}, {{2.0, 2.0}, 0.0}, {{2.0, 0.0}, 0.0}, {{0.0, 2.0}, 0.0}});
}

// A square with a notch from the top: the concave vertex is (2, 2).
Loop notched() {
    return polygon({{{0.0, 0.0}, 0.0},
                    {{4.0, 0.0}, 0.0},
                    {{4.0, 4.0}, 0.0},
                    {{2.0, 2.0}, 0.0},
                    {{0.0, 4.0}, 0.0}});
}

Loop diamond() {
    return polygon({{{0.0, -1.0}, 0.0}, {{1.0, 0.0}, 0.0}, {{0.0, 1.0}, 0.0}, {{-1.0, 0.0}, 0.0}});
}

// --- loopPieces ------------------------------------------------------------

TEST(Winding, describesALoopByItsPieces) {
    // A repeated vertex gives a piece that is a point, left out; the flag
    // closed is not read.
    const Loop loop = Polyline{{{{1.0, 1.0}, 0.0},
                                {{1.0, 1.0}, 0.0},
                                {{5.0, 1.0}, 0.5},
                                {{5.0, 3.0}, 0.0},
                                {{1.0, 3.0}, 0.0}},
                               false};
    const auto pieces = detail::loopPieces(loop);
    ASSERT_EQ(pieces.pieces.size(), 4U);
    std::vector<std::size_t> indices;
    indices.reserve(pieces.pieces.size());
    for (const auto& piece : pieces.pieces) {
        indices.push_back(piece.index);
    }
    EXPECT_EQ(indices, (std::vector<std::size_t>{1, 2, 3, 4}));
    EXPECT_TRUE(std::holds_alternative<detail::ArcMeaning>(pieces.pieces[1].meaning.support));
    EXPECT_TRUE(std::holds_alternative<detail::LineMeaning>(pieces.pieces[3].meaning.support));

    const auto circle = detail::loopPieces(Circle{{0.0, 0.0}, 2.0});
    ASSERT_EQ(circle.pieces.size(), 1U);
    EXPECT_EQ(circle.pieces[0].index, 0U);
}

// --- pointStatus -------------------------------------------------------------

TEST(Winding, placesPointsInsideOutsideAndOnARectangle) {
    const std::vector<Loop> loops{rectangle(1.0, 1.0, 5.0, 3.0)};
    EXPECT_EQ(stateOf(3.0, 2.0, loops), kIn);
    EXPECT_EQ(stateOf(6.0, 2.0, loops), kOut);
    EXPECT_EQ(stateOf(0.0, 2.0, loops), kOut);
    EXPECT_EQ(stateOf(3.0, 0.0, loops), kOut);
    EXPECT_EQ(stateOf(3.0, 4.0, loops), kOut);
    EXPECT_EQ(stateOf(3.0, 1.0, loops), kOn);
    EXPECT_EQ(piecesAt(3.0, 1.0, loops), (std::vector<PieceRef>{{0, 0}}));
    // A vertex lies on the two pieces that meet there.
    EXPECT_EQ(piecesAt(5.0, 3.0, loops), (std::vector<PieceRef>{{0, 1}, {0, 2}}));
    EXPECT_EQ(piecesAt(1.0, 1.0, loops), (std::vector<PieceRef>{{0, 0}, {0, 3}}));
}

TEST(Winding, readsTheRayThroughTheVerticesOfARectangle) {
    // The half-lines toward +x run along the top and bottom sides.
    const std::vector<Loop> loops{rectangle(1.0, 1.0, 5.0, 3.0)};
    EXPECT_EQ(stateOf(0.0, 3.0, loops), kOut);
    EXPECT_EQ(stateOf(0.0, 1.0, loops), kOut);
    EXPECT_EQ(stateOf(6.0, 3.0, loops), kOut);
    EXPECT_EQ(stateOf(6.0, 1.0, loops), kOut);
}

TEST(Winding, decidesPointsOneUlpFromASide) {
    const std::vector<Loop> loops{rectangle(1.0, 1.0, 5.0, 3.0)};
    EXPECT_EQ(stateOf(3.0, below(1.0), loops), kOut);
    EXPECT_EQ(stateOf(3.0, above(1.0), loops), kIn);
    EXPECT_EQ(stateOf(below(5.0), 2.0, loops), kIn);
    EXPECT_EQ(stateOf(above(5.0), 2.0, loops), kOut);
    EXPECT_EQ(stateOf(below(1.0), 3.0, loops), kOut);
    EXPECT_EQ(stateOf(above(1.0), below(3.0), loops), kIn);
}

TEST(Winding, placesPointsAroundACircle) {
    const std::vector<Loop> loops{Circle{{0.0, 0.0}, 5.0}};
    EXPECT_EQ(stateOf(0.0, 0.0, loops), kIn);
    EXPECT_EQ(stateOf(3.0, 4.0, loops), kOn);
    EXPECT_EQ(piecesAt(3.0, 4.0, loops), (std::vector<PieceRef>{{0, 0}}));
    EXPECT_EQ(stateOf(-4.0, -3.0, loops), kOn);
    // One ulp inside and outside: 3^2 + 4^2 = 25.
    EXPECT_EQ(stateOf(3.0, below(4.0), loops), kIn);
    EXPECT_EQ(stateOf(3.0, above(4.0), loops), kOut);
    EXPECT_EQ(stateOf(below(-4.0), 3.0, loops), kOut);
    EXPECT_EQ(stateOf(above(-4.0), 3.0, loops), kIn);
    // The half-line toward +x is tangent at the top or at the bottom.
    EXPECT_EQ(stateOf(-7.0, 5.0, loops), kOut);
    EXPECT_EQ(stateOf(-7.0, -5.0, loops), kOut);
    EXPECT_EQ(stateOf(0.0, below(5.0), loops), kIn);
    EXPECT_EQ(stateOf(0.0, above(-5.0), loops), kIn);
    // It crosses twice, or once from inside.
    EXPECT_EQ(stateOf(-7.0, 4.0, loops), kOut);
    EXPECT_EQ(stateOf(-2.0, 4.0, loops), kIn);
    EXPECT_EQ(stateOf(7.0, 0.0, loops), kOut);
}

TEST(Winding, placesPointsWithARoot) {
    const std::vector<Loop> loops{Circle{{0.0, 0.0}, 5.0}};
    const Curve diagonal = Segment{{0.0, 0.0}, {1.0, 1.0}};
    // (5, 5) / sqrt(2) lies on the circle; (1, 1) / sqrt(2) inside, and
    // (10, 10) / sqrt(2) outside.
    const ExactPoint on = meetingWithXAbove(diagonal, Circle{{0.0, 0.0}, 5.0}, 0.0);
    ASSERT_FALSE(on.radicand.isZero());
    EXPECT_EQ(stateOf(on, loops), kOn);
    EXPECT_EQ(stateOf(meetingWithXAbove(diagonal, Circle{{0.0, 0.0}, 1.0}, 0.0), loops), kIn);
    EXPECT_EQ(stateOf(meetingWithXAbove(diagonal, Circle{{0.0, 0.0}, 10.0}, 0.0), loops), kOut);
    EXPECT_EQ(stateOf(meetingWithXBelow(diagonal, Circle{{0.0, 0.0}, 10.0}, 0.0), loops), kOut);
    // The exact end of an Arc of radius 5 lies on the circle, one of radius
    // 4 inside, one of radius 6 outside.
    const auto endOf = [](double radius) {
        return detail::endPoint(detail::meaningOf(Arc{{0.0, 0.0}, radius, 0.7, 1.9}));
    };
    EXPECT_EQ(stateOf(endOf(5.0), loops), kOn);
    EXPECT_EQ(stateOf(endOf(4.0), loops), kIn);
    EXPECT_EQ(stateOf(endOf(6.0), loops), kOut);
}

TEST(Winding, placesPointsAroundAPolylineWithBulges) {
    const std::vector<Loop> loops{stadium()};
    EXPECT_EQ(stateOf(2.0, 1.0, loops), kIn);
    EXPECT_EQ(stateOf(4.5, 1.0, loops), kIn);
    EXPECT_EQ(stateOf(5.5, 1.0, loops), kOut);
    EXPECT_EQ(stateOf(-0.5, 0.5, loops), kIn);
    EXPECT_EQ(stateOf(-0.5, 1.9, loops), kOut);
    EXPECT_EQ(piecesAt(5.0, 1.0, loops), (std::vector<PieceRef>{{0, 1}}));
    EXPECT_EQ(piecesAt(-1.0, 1.0, loops), (std::vector<PieceRef>{{0, 3}}));
    EXPECT_EQ(stateOf(below(5.0), 1.0, loops), kIn);
    EXPECT_EQ(stateOf(above(5.0), 1.0, loops), kOut);
    EXPECT_EQ(stateOf(above(-1.0), 1.0, loops), kIn);
    EXPECT_EQ(stateOf(below(-1.0), 1.0, loops), kOut);
    // Along the top side, where both arcs have their top point at a vertex.
    EXPECT_EQ(stateOf(-2.0, 2.0, loops), kOut);
    EXPECT_EQ(stateOf(4.5, 2.0, loops), kOut);
    EXPECT_EQ(stateOf(-2.0, 0.0, loops), kOut);
    EXPECT_EQ(stateOf(2.0, below(2.0), loops), kIn);
    // The line from (4, 1) with direction (1, 1) meets the circle of the
    // right arc on the arc (x > 4) and off it, inside the rectangle (x < 4).
    const Curve ray = Segment{{4.0, 1.0}, {5.0, 2.0}};
    const Curve rightCircle = Circle{{4.0, 1.0}, 1.0};
    const auto status =
        detail::pointStatus(meetingWithXAbove(ray, rightCircle, 4.0), describe(loops));
    EXPECT_EQ(status.state, kOn);
    EXPECT_EQ(status.pieces, (std::vector<PieceRef>{{0, 1}}));
    EXPECT_EQ(stateOf(meetingWithXBelow(ray, rightCircle, 4.0), loops), kIn);
}

TEST(Winding, readsTheRayTangentToAnArcInsideAPiece) {
    // The bottom of the arc, (2, -2), is inside the piece.
    const std::vector<Loop> loops{lowerHalfDisc()};
    EXPECT_EQ(stateOf(-1.0, -2.0, loops), kOut);
    EXPECT_EQ(stateOf(1.0, -1.0, loops), kIn);
    EXPECT_EQ(stateOf(-1.0, -1.0, loops), kOut);
    EXPECT_EQ(stateOf(5.0, -1.0, loops), kOut);
    EXPECT_EQ(stateOf(2.0, above(-2.0), loops), kIn);
    EXPECT_EQ(stateOf(2.0, below(-2.0), loops), kOut);
    EXPECT_EQ(piecesAt(2.0, -2.0, loops), (std::vector<PieceRef>{{0, 0}}));
    EXPECT_EQ(piecesAt(3.0, 0.0, loops), (std::vector<PieceRef>{{0, 1}}));
    EXPECT_EQ(stateOf(3.0, above(0.0), loops), kOut);
    EXPECT_EQ(stateOf(3.0, -0.5, loops), kIn);
    // Along the segment, through both of its ends.
    EXPECT_EQ(stateOf(-1.0, 0.0, loops), kOut);
}

TEST(Winding, readsTheRayThroughConvexAndConcaveVertices) {
    const std::vector<Loop> square{diamond()};
    // Touching the top vertex, or the bottom one, from outside.
    EXPECT_EQ(stateOf(-2.0, 1.0, square), kOut);
    EXPECT_EQ(stateOf(-2.0, -1.0, square), kOut);
    // Through the right vertex from inside, through both side vertices from
    // outside.
    EXPECT_EQ(stateOf(-0.5, 0.0, square), kIn);
    EXPECT_EQ(stateOf(-2.0, 0.0, square), kOut);
    EXPECT_EQ(stateOf(0.0, 0.0, square), kIn);

    const std::vector<Loop> notch{notched()};
    // Through the concave vertex (2, 2), inside on both sides of it.
    EXPECT_EQ(stateOf(1.0, 2.0, notch), kIn);
    EXPECT_EQ(stateOf(3.0, 2.0, notch), kIn);
    EXPECT_EQ(stateOf(-1.0, 2.0, notch), kOut);
    EXPECT_EQ(stateOf(2.0, 3.0, notch), kOut);
    EXPECT_EQ(stateOf(2.0, below(2.0), notch), kIn);
    EXPECT_EQ(stateOf(2.0, above(2.0), notch), kOut);
    EXPECT_EQ(piecesAt(2.0, 2.0, notch), (std::vector<PieceRef>{{0, 2}, {0, 3}}));
}

TEST(Winding, appliesTheEvenOddRuleToASelfIntersectingPolyline) {
    const std::vector<Loop> loops{bowTie()};
    EXPECT_EQ(stateOf(1.5, 1.0, loops), kIn);
    EXPECT_EQ(stateOf(0.5, 1.0, loops), kIn);
    EXPECT_EQ(stateOf(1.0, 0.5, loops), kOut);
    EXPECT_EQ(stateOf(1.0, 1.5, loops), kOut);
    // The half-line through the crossing.
    EXPECT_EQ(stateOf(-1.0, 1.0, loops), kOut);
    EXPECT_EQ(piecesAt(1.0, 1.0, loops), (std::vector<PieceRef>{{0, 0}, {0, 2}}));
}

TEST(Winding, cancelsTwoLoopsAlongACommonSide) {
    const std::vector<Loop> loops{rectangle(0.0, 0.0, 2.0, 2.0), rectangle(2.0, 0.0, 4.0, 2.0)};
    EXPECT_EQ(stateOf(1.0, 1.0, loops), kIn);
    EXPECT_EQ(stateOf(3.0, 1.0, loops), kIn);
    EXPECT_EQ(stateOf(-1.0, 1.0, loops), kOut);
    EXPECT_EQ(stateOf(-1.0, 2.0, loops), kOut);
    EXPECT_EQ(stateOf(5.0, 1.0, loops), kOut);
    EXPECT_EQ(piecesAt(2.0, 1.0, loops), (std::vector<PieceRef>{{0, 1}, {1, 3}}));
    // Both sides of the common side are inside.
    const auto states = detail::sectorStates(at(2.0, 1.0), describe(loops));
    ASSERT_EQ(states.order.groupCount(), 2U);
    EXPECT_EQ(states.loopHalfPieces(), 4U);
    EXPECT_EQ(states.inside, (std::vector<bool>{true, true}));

    // The same square twice cancels everywhere.
    const std::vector<Loop> twice{rectangle(0.0, 0.0, 2.0, 2.0), rectangle(0.0, 0.0, 2.0, 2.0)};
    EXPECT_EQ(stateOf(1.0, 1.0, twice), kOut);
    EXPECT_EQ(sectorsOf(at(2.0, 1.0), twice), (std::vector<bool>{false, false}));
}

TEST(Winding, alternatesThroughNestedLoops) {
    const std::vector<Loop> loops{rectangle(0.0, 0.0, 10.0, 10.0), rectangle(2.0, 2.0, 8.0, 8.0),
                                  Circle{{5.0, 5.0}, 1.0}};
    EXPECT_EQ(stateOf(1.0, 5.0, loops), kIn);
    EXPECT_EQ(stateOf(3.0, 5.0, loops), kOut);
    EXPECT_EQ(stateOf(5.0, 5.0, loops), kIn);
    EXPECT_EQ(stateOf(-1.0, 5.0, loops), kOut);
    EXPECT_EQ(stateOf(5.0, 6.0, loops), kOn);
    EXPECT_EQ(stateOf(5.0, above(6.0), loops), kOut);
    EXPECT_EQ(stateOf(5.0, below(6.0), loops), kIn);
    // On the middle loop: inside on the outer side, outside on the inner one.
    const auto states = detail::sectorStates(at(2.0, 5.0), describe(loops));
    ASSERT_EQ(states.order.groupCount(), 2U);
    // Group 0 is up (90 degrees), group 1 down: sector 0 is the left side
    // (x < 2), sector 1 the right side.
    EXPECT_EQ(states.inside, (std::vector<bool>{true, false}));
}

// --- sectorStates ------------------------------------------------------------

TEST(Winding, givesTheSectorsWhereTwoLoopsTouchAtAVertex) {
    const std::vector<Loop> loops{rectangle(0.0, 0.0, 2.0, 2.0), rectangle(2.0, 2.0, 4.0, 4.0)};
    const auto states = detail::sectorStates(at(2.0, 2.0), describe(loops));
    // Right (the bottom of the upper square), up (its left side), left (the
    // top of the lower square), down (its right side).
    ASSERT_EQ(states.order.groupCount(), 4U);
    EXPECT_EQ(states.loopHalfPieces(), 4U);
    std::vector<PieceRef> owners;
    for (std::size_t g = 0; g < 4; ++g) {
        ASSERT_EQ(states.order.group(g).size(), 1U);
        owners.push_back(states.owners[states.order.group(g)[0]]);
    }
    EXPECT_EQ(owners, (std::vector<PieceRef>{{1, 0}, {1, 3}, {0, 2}, {0, 1}}));
    EXPECT_EQ(states.inside, (std::vector<bool>{true, false, true, false}));
    EXPECT_EQ(stateOf(2.0, 2.0, loops), kOn);
}

TEST(Winding, givesTheSectorsAtAVertexOfARectangle) {
    const std::vector<Loop> loops{rectangle(0.0, 0.0, 4.0, 2.0)};
    // Top right: left (180) and down (270); inside between them.
    EXPECT_EQ(sectorsOf(at(4.0, 2.0), loops), (std::vector<bool>{true, false}));
    // Bottom right: up (90) and left (180).
    EXPECT_EQ(sectorsOf(at(4.0, 0.0), loops), (std::vector<bool>{true, false}));
    // Bottom left: right (0) and up (90).
    EXPECT_EQ(sectorsOf(at(0.0, 0.0), loops), (std::vector<bool>{true, false}));
    // Top left: right (0) and down (270): inside from down to right.
    EXPECT_EQ(sectorsOf(at(0.0, 2.0), loops), (std::vector<bool>{false, true}));
}

TEST(Winding, addsReferencesToTheSectors) {
    const std::vector<Loop> loops{rectangle(0.0, 0.0, 4.0, 2.0)};
    const Curve vertical = Segment{{2.0, -1.0}, {2.0, 3.0}};
    const std::vector<HalfPiece> references{half(vertical, Heading::forward),
                                            half(vertical, Heading::backward)};
    // On the bottom side: right, up (reference), left, down (reference).
    const auto states = detail::sectorStates(at(2.0, 0.0), describe(loops), references);
    ASSERT_EQ(states.halfPieces.size(), 4U);
    EXPECT_EQ(states.loopHalfPieces(), 2U);
    ASSERT_EQ(states.order.groupCount(), 4U);
    EXPECT_EQ(states.order.groupOf(2), 1U);
    EXPECT_EQ(states.order.groupOf(3), 3U);
    EXPECT_EQ(states.inside, (std::vector<bool>{true, true, false, false}));
    // Inside, off the loops: every sector has the state of the point.
    EXPECT_EQ(sectorsOf(at(2.0, 1.0), loops, references), (std::vector<bool>{true, true}));
    EXPECT_EQ(sectorsOf(at(2.0, 1.0), loops), (std::vector<bool>{true}));
    EXPECT_EQ(sectorsOf(at(2.0, 5.0), loops), (std::vector<bool>{false}));
}

TEST(Winding, separatesACircleFromItsTangent) {
    // At (1, 0) on the unit circle, with the tangent line x = 1: up the line
    // (curvature 0) comes before the circle counterclockwise (+1); down, the
    // circle clockwise (-1) before the line. Only the sector between the two
    // halves of the circle, through 180 degrees, is inside.
    const std::vector<Loop> loops{Circle{{0.0, 0.0}, 1.0}};
    const Curve tangent = Segment{{1.0, -1.0}, {1.0, 1.0}};
    const std::vector<HalfPiece> references{half(tangent, Heading::forward),
                                            half(tangent, Heading::backward)};
    const auto states = detail::sectorStates(at(1.0, 0.0), describe(loops), references);
    ASSERT_EQ(states.order.groupCount(), 4U);
    EXPECT_EQ(states.order.groupOf(2), 0U);
    EXPECT_EQ(states.order.groupOf(3), 3U);
    EXPECT_EQ(states.inside, (std::vector<bool>{false, true, false, false}));
    // Without the tangent: inside, then outside through 0 degrees.
    EXPECT_EQ(sectorsOf(at(1.0, 0.0), loops), (std::vector<bool>{true, false}));
    // At the top, where the half-line toward +x is tangent: the circle runs
    // clockwise toward +x (group 0) and counterclockwise toward -x (group 1);
    // outside above, inside below.
    EXPECT_EQ(sectorsOf(at(0.0, 1.0), loops), (std::vector<bool>{false, true}));
    EXPECT_EQ(sectorsOf(at(0.0, -1.0), loops), (std::vector<bool>{true, false}));
}

TEST(Winding, givesTheSectorsAtAPointWithARoot) {
    // On the unit circle at (1, 1) / sqrt(2), with the diagonal as reference:
    // the diagonal toward the centre (225 degrees) is inside, the other way
    // (45) outside. Groups: 45 (diagonal out), 135 (circle counterclockwise),
    // 225 (diagonal in), 315 (circle clockwise).
    const std::vector<Loop> loops{Circle{{0.0, 0.0}, 1.0}};
    const Curve diagonal = Segment{{0.0, 0.0}, {1.0, 1.0}};
    const ExactPoint point = meetingWithXAbove(diagonal, Circle{{0.0, 0.0}, 1.0}, 0.0);
    const std::vector<HalfPiece> references{half(diagonal, Heading::forward),
                                            half(diagonal, Heading::backward)};
    EXPECT_EQ(sectorsOf(point, loops, references), (std::vector<bool>{false, true, true, false}));
    EXPECT_EQ(sectorsOf(point, loops), (std::vector<bool>{true, false}));
}

// --- Independence ------------------------------------------------------------

// The loop run the other way: vertices in reverse order, each bulge moved to
// the vertex its segment now starts from, with the opposite sign. A Circle
// cannot be reversed.
Loop reversedLoop(const Loop& loop) {
    if (std::holds_alternative<Circle>(loop)) {
        return loop;
    }
    const auto& vertices = std::get<Polyline>(loop).vertices;
    const std::size_t n = vertices.size();
    Polyline result{{}, true};
    for (std::size_t k = 0; k < n; ++k) {
        // The new segment k is the old segment n - 2 - k (mod n) reversed.
        const std::size_t old = (2 * n - 2 - k) % n;
        result.vertices.push_back({vertices[n - 1 - k].position, -vertices[old].bulge});
    }
    return result;
}

// The loop starting from another vertex.
Loop rotatedLoop(const Loop& loop, std::size_t first) {
    if (std::holds_alternative<Circle>(loop)) {
        return loop;
    }
    const auto& vertices = std::get<Polyline>(loop).vertices;
    Polyline result{{}, true};
    for (std::size_t k = 0; k < vertices.size(); ++k) {
        result.vertices.push_back(vertices[(first + k) % vertices.size()]);
    }
    return result;
}

struct Answers {
    std::vector<PointState> states;
    std::vector<std::vector<bool>> sectors;

    friend bool operator==(const Answers&, const Answers&) = default;
};

Answers answersFor(std::span<const Loop> loops, std::span<const ExactPoint> points,
                   std::span<const HalfPiece> references) {
    const auto pieces = describe(loops);
    Answers answers;
    for (const auto& point : points) {
        answers.states.push_back(detail::pointStatus(point, pieces).state);
        answers.sectors.push_back(detail::sectorStates(point, pieces, references).inside);
    }
    return answers;
}

void expectIndependence(const std::vector<Loop>& loops, const std::vector<ExactPoint>& points,
                        std::span<const HalfPiece> references = {}) {
    const Answers expected = answersFor(loops, points, references);
    std::vector<std::size_t> permutation(loops.size());
    std::iota(permutation.begin(), permutation.end(), std::size_t{0});
    bool more = true;
    while (more) {
        for (std::size_t variant = 0; variant < 4; ++variant) {
            std::vector<Loop> changed;
            for (std::size_t k = 0; k < loops.size(); ++k) {
                Loop loop = loops[permutation[k]];
                if ((variant & 1U) != 0) {
                    loop = reversedLoop(loop);
                }
                if ((variant & 2U) != 0) {
                    loop = rotatedLoop(loop, k + 1);
                }
                changed.push_back(loop);
            }
            EXPECT_EQ(answersFor(changed, points, references), expected) << "variant " << variant;
        }
        more = std::next_permutation(permutation.begin(), permutation.end());
    }
}

TEST(Winding, doesNotDependOnOrderDirectionOrFirstVertex) {
    expectIndependence(
        {rectangle(0.0, 0.0, 10.0, 10.0), rectangle(2.0, 2.0, 8.0, 8.0), Circle{{5.0, 5.0}, 1.0}},
        {at(1.0, 5.0), at(3.0, 5.0), at(5.0, 5.0), at(-1.0, 5.0), at(2.0, 5.0), at(2.0, 2.0),
         at(5.0, 6.0), at(5.0, 4.0), at(-1.0, 2.0)});
    expectIndependence({rectangle(0.0, 0.0, 2.0, 2.0), rectangle(2.0, 0.0, 4.0, 2.0),
                        rectangle(2.0, 2.0, 4.0, 4.0)},
                       {at(1.0, 1.0), at(2.0, 1.0), at(2.0, 2.0), at(3.0, 2.0), at(2.0, 0.0),
                        at(-1.0, 2.0), at(-1.0, 0.0), at(3.0, 3.0)});
    expectIndependence({stadium(), lowerHalfDisc()},
                       {at(2.0, 1.0), at(5.0, 1.0), at(4.0, 0.0), at(2.0, 0.0), at(2.0, -2.0),
                        at(-2.0, 2.0), at(-2.0, 0.0), at(0.0, 0.0), at(2.0, -1.0)});
    expectIndependence({bowTie(), notched()},
                       {at(1.0, 1.0), at(1.5, 1.0), at(2.0, 2.0), at(-1.0, 1.0), at(3.0, 2.0),
                        at(2.0, 0.0), at(1.0, 0.5)});
    const Curve vertical = Segment{{2.0, -1.0}, {2.0, 3.0}};
    const std::vector<HalfPiece> references{half(vertical, Heading::forward),
                                            half(vertical, Heading::backward)};
    expectIndependence({rectangle(0.0, 0.0, 4.0, 2.0), stadium()},
                       {at(2.0, 0.0), at(2.0, 2.0), at(2.0, 1.0), at(2.0, -1.0)}, references);
}

}  // namespace
