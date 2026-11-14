// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Unit tests of the arrangement of curves (stage 0.3, package PS6). The data
// have small integer coordinates (or the exact meetings of such curves), so
// every face, piece and order expected below can be seen on a sketch: the
// faces are described by the curves of their darts and the direction in which
// each dart runs along its curve.

#include "arrangement.hpp"

#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "positions.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using solid_space::geom2d::Circle;
using solid_space::geom2d::Curve;
using solid_space::geom2d::Point2;
using solid_space::geom2d::PolylineSegment;
using solid_space::geom2d::Segment;
using solid_space::geom2d::detail::Arrangement;
using solid_space::geom2d::detail::ArrangementCurve;
using solid_space::geom2d::detail::Box;
using solid_space::geom2d::detail::ExactPoint;
using solid_space::robust::Sign;

namespace detail = solid_space::geom2d::detail;

ArrangementCurve given(const Curve& curve, std::size_t priority) {
    ArrangementCurve result;
    result.meaning = detail::meaningOf(curve);
    result.priority = priority;
    return result;
}

// The curves with their index as priority.
std::vector<ArrangementCurve> given(std::initializer_list<Curve> curves) {
    std::vector<ArrangementCurve> result;
    result.reserve(curves.size());
    for (const Curve& curve : curves) {
        result.push_back(given(curve, result.size()));
    }
    return result;
}

ExactPoint at(double x, double y) {
    return detail::exactPoint(Point2{x, y});
}

// A dart as the tests read it: its curve and whether it runs against it.
using Step = std::pair<std::size_t, bool>;
using Steps = std::vector<Step>;

// The darts of a cycle or a loop, as a sorted list of steps.
Steps stepsOf(const Arrangement& arrangement, std::span<const std::size_t> darts) {
    Steps steps;
    for (const std::size_t dart : darts) {
        steps.emplace_back(arrangement.pieces()[Arrangement::pieceOf(dart)].curve,
                           Arrangement::reversed(dart));
    }
    std::sort(steps.begin(), steps.end());
    return steps;
}

// The face of a point as steps, or nothing for the unbounded face.
std::optional<Steps> faceSteps(const Arrangement& arrangement, double x, double y) {
    const auto face = arrangement.faceOf(at(x, y));
    if (!face) {
        return std::nullopt;
    }
    return stepsOf(arrangement, arrangement.cycle(*face));
}

// The bounded face of a point, which the test expects to exist.
std::size_t boundedFaceAt(const Arrangement& arrangement, double x, double y) {
    const auto face = arrangement.faceOf(at(x, y));
    EXPECT_TRUE(face.has_value());
    return face.value_or(0);
}

std::size_t boundedFaces(const Arrangement& arrangement) {
    std::size_t count = 0;
    for (std::size_t c = 0; c < arrangement.cycleCount(); ++c) {
        if (!arrangement.isHole(c)) {
            ++count;
        }
    }
    return count;
}

Point2 rounded(const Arrangement& arrangement, std::size_t vertex) {
    return detail::nearestPoint(arrangement.vertex(vertex));
}

Segment segment(double x0, double y0, double x1, double y1) {
    return Segment{Point2{x0, y0}, Point2{x1, y1}};
}

// The four sides of an axis-aligned rectangle, counterclockwise from (x0, y0).
std::vector<Curve> rectangle(double x0, double y0, double x1, double y1) {
    return {segment(x0, y0, x1, y0), segment(x1, y0, x1, y1), segment(x1, y1, x0, y1),
            segment(x0, y1, x0, y0)};
}

std::vector<ArrangementCurve> given(const std::vector<Curve>& curves) {
    std::vector<ArrangementCurve> result;
    result.reserve(curves.size());
    for (const Curve& curve : curves) {
        result.push_back(given(curve, result.size()));
    }
    return result;
}

TEST(Arrangement, NoCurves) {
    const Arrangement arrangement(std::span<const ArrangementCurve>{});
    EXPECT_EQ(arrangement.vertexCount(), 0U);
    EXPECT_EQ(arrangement.dartCount(), 0U);
    EXPECT_EQ(arrangement.cycleCount(), 0U);
    EXPECT_EQ(arrangement.groupCount(), 0U);
    EXPECT_EQ(arrangement.faceOf(at(1, 2)), std::nullopt);
}

TEST(Arrangement, RectangleWithACircleInside) {
    std::vector<Curve> curves = rectangle(0, 0, 4, 3);
    curves.emplace_back(Circle{Point2{2, 1.5}, 1});
    const auto input = given(curves);
    const Arrangement arrangement(input);

    // Four corners and the start of the circle, which meets nothing.
    ASSERT_EQ(arrangement.vertexCount(), 5U);
    EXPECT_EQ(arrangement.pieces().size(), 5U);
    EXPECT_EQ(arrangement.groupCount(), 2U);
    EXPECT_EQ(arrangement.groupOfCurve(0), 0U);
    EXPECT_EQ(arrangement.groupOfCurve(3), 0U);
    EXPECT_EQ(arrangement.groupOfCurve(4), 1U);
    EXPECT_EQ(rounded(arrangement, arrangement.groupVertex(0)).x, 0.0);
    EXPECT_EQ(rounded(arrangement, arrangement.groupVertex(1)).x, 3.0);
    EXPECT_EQ(boundedFaces(arrangement), 2U);

    const Steps sides{{0, false}, {1, false}, {2, false}, {3, false}};
    EXPECT_EQ(faceSteps(arrangement, 0.5, 0.5), sides);
    EXPECT_EQ(faceSteps(arrangement, 2, 1.5), (Steps{{4, false}}));
    EXPECT_EQ(faceSteps(arrangement, 5, 1), std::nullopt);

    const auto rectangleHole = arrangement.holeCycle(0);
    const auto circleHole = arrangement.holeCycle(1);
    EXPECT_EQ(stepsOf(arrangement, arrangement.cycle(rectangleHole)),
              (Steps{{0, true}, {1, true}, {2, true}, {3, true}}));
    EXPECT_EQ(stepsOf(arrangement, arrangement.cycle(circleHole)), (Steps{{4, true}}));

    const auto contour = arrangement.simpleLoops(boundedFaceAt(arrangement, 0.5, 0.5));
    ASSERT_EQ(contour.size(), 1U);
    EXPECT_TRUE(arrangement.counterclockwise(contour[0]));
    const auto island = arrangement.simpleLoops(circleHole);
    ASSERT_EQ(island.size(), 1U);
    EXPECT_FALSE(arrangement.counterclockwise(island[0]));
    EXPECT_TRUE(arrangement.encloses(contour[0], island[0]));
    EXPECT_FALSE(arrangement.encloses(island[0], contour[0]));
    EXPECT_TRUE(arrangement.inside(at(1, 1), contour[0]));
    EXPECT_FALSE(arrangement.inside(at(1, 1), island[0]));
    EXPECT_TRUE(arrangement.inside(at(2.5, 1.5), island[0]));
}

TEST(Arrangement, LinesBeyondACornerHaveTheSameFaceOnBothSides) {
    // The bottom side runs on to x = 5, the right side down to y = -1.
    const auto input = given(
        {segment(0, 0, 5, 0), segment(4, -1, 4, 3), segment(4, 3, 0, 3), segment(0, 3, 0, 0)});
    const Arrangement arrangement(input);
    EXPECT_EQ(arrangement.pieces().size(), 6U);
    EXPECT_EQ(boundedFaces(arrangement), 1U);
    EXPECT_EQ(faceSteps(arrangement, 1, 1),
              (Steps{{0, false}, {1, false}, {2, false}, {3, false}}));

    const auto hole = arrangement.holeCycle(0);
    // The hole cycle runs out and back along both protruding pieces.
    EXPECT_EQ(arrangement.cycle(hole).size(), 8U);
    const auto loops = arrangement.simpleLoops(hole);
    ASSERT_EQ(loops.size(), 1U);
    EXPECT_EQ(stepsOf(arrangement, loops[0]), (Steps{{0, true}, {1, true}, {2, true}, {3, true}}));
    EXPECT_FALSE(arrangement.counterclockwise(loops[0]));
    for (const std::size_t dart : loops[0]) {
        // No dart of the loop leaves the square.
        const Point2 from = rounded(arrangement, arrangement.tail(dart));
        EXPECT_LE(from.x, 4.0);
        EXPECT_GE(from.y, 0.0);
    }
}

TEST(Arrangement, VertexWithSixPieces) {
    // A square, its two diagonals and the horizontal line through its
    // centre (1, 1).
    std::vector<Curve> curves = rectangle(0, 0, 2, 2);
    curves.emplace_back(segment(0, 0, 2, 2));
    curves.emplace_back(segment(2, 0, 0, 2));
    curves.emplace_back(segment(0, 1, 2, 1));
    const auto input = given(curves);
    const Arrangement arrangement(input);
    EXPECT_EQ(boundedFaces(arrangement), 6U);

    std::optional<std::size_t> centre;
    for (std::size_t v = 0; v < arrangement.vertexCount(); ++v) {
        if (rounded(arrangement, v).x == 1.0 && rounded(arrangement, v).y == 1.0) {
            centre = v;
        }
    }
    ASSERT_TRUE(centre.has_value());
    // Counterclockwise from +x: toward (2, 1), (2, 2), (0, 2), (0, 1),
    // (0, 0), (2, 0).
    const std::vector<Point2> expected{{2, 1}, {2, 2}, {0, 2}, {0, 1}, {0, 0}, {2, 0}};
    const auto darts = arrangement.around(centre.value_or(0));
    ASSERT_EQ(darts.size(), expected.size());
    for (std::size_t k = 0; k < darts.size(); ++k) {
        const Point2 head = rounded(arrangement, arrangement.head(darts[k]));
        EXPECT_EQ(head.x, expected[k].x) << k;
        EXPECT_EQ(head.y, expected[k].y) << k;
    }
    // The triangle below the centre on the right: the bottom-right half of
    // the lower square, between the line y = 1, the right side and the
    // diagonal from (2, 0).
    EXPECT_EQ(faceSteps(arrangement, 1.8, 0.9), (Steps{{1, false}, {5, true}, {6, true}}));
    EXPECT_EQ(faceSteps(arrangement, 1, 0.3), (Steps{{0, false}, {4, true}, {5, false}}));
}

TEST(Arrangement, CirclesTangentInsideAndOutsideAtOnePoint) {
    // A of radius 2 around the origin; B of radius 1 inside it and C of
    // radius 1 outside it, both tangent to A at (2, 0).
    const auto input =
        given({Circle{Point2{0, 0}, 2}, Circle{Point2{1, 0}, 1}, Circle{Point2{3, 0}, 1}});
    const Arrangement arrangement(input);
    ASSERT_EQ(arrangement.vertexCount(), 1U);
    EXPECT_EQ(rounded(arrangement, 0).x, 2.0);
    EXPECT_EQ(arrangement.groupCount(), 1U);
    EXPECT_EQ(arrangement.pieces().size(), 3U);

    // Toward +y by increasing curvature: C backward (-1), A forward (1/2),
    // B forward (1); toward -y: B backward, A backward, C forward.
    const Steps expected{{2, true}, {0, false}, {1, false}, {1, true}, {0, true}, {2, false}};
    Steps order;
    for (const std::size_t dart : arrangement.around(0)) {
        order.emplace_back(arrangement.pieces()[Arrangement::pieceOf(dart)].curve,
                           Arrangement::reversed(dart));
    }
    EXPECT_EQ(order, expected);

    EXPECT_EQ(boundedFaces(arrangement), 3U);
    EXPECT_EQ(faceSteps(arrangement, 1, 0.5), (Steps{{1, false}}));
    EXPECT_EQ(faceSteps(arrangement, -1, 0), (Steps{{0, false}, {1, true}}));
    EXPECT_EQ(faceSteps(arrangement, 3, 0), (Steps{{2, false}}));
    EXPECT_EQ(faceSteps(arrangement, 5, 0), std::nullopt);

    // The crescent between A and B touches itself at (2, 0): two loops, the
    // contour counterclockwise and the island B clockwise, which it
    // encloses although every vertex of B is on it.
    const auto loops = arrangement.simpleLoops(boundedFaceAt(arrangement, -1, 0));
    ASSERT_EQ(loops.size(), 2U);
    const std::size_t outer = arrangement.counterclockwise(loops[0]) ? 0 : 1;
    EXPECT_TRUE(arrangement.counterclockwise(loops[outer]));
    EXPECT_FALSE(arrangement.counterclockwise(loops[1 - outer]));
    EXPECT_EQ(stepsOf(arrangement, loops[outer]), (Steps{{0, false}}));
    EXPECT_TRUE(arrangement.encloses(loops[outer], loops[1 - outer]));
    EXPECT_FALSE(arrangement.encloses(loops[1 - outer], loops[outer]));

    // The hole cycle runs around A and C, which touch from outside.
    const auto hole = arrangement.simpleLoops(arrangement.holeCycle(0));
    ASSERT_EQ(hole.size(), 2U);
    EXPECT_FALSE(arrangement.counterclockwise(hole[0]));
    EXPECT_FALSE(arrangement.counterclockwise(hole[1]));
    EXPECT_FALSE(arrangement.encloses(hole[0], hole[1]));
    EXPECT_FALSE(arrangement.encloses(hole[1], hole[0]));
}

TEST(Arrangement, StretchesSharedByThreeCurvesAndALink) {
    // On the x axis: a link (priority 10) from 4 to 5, curve 1 from 0 to 4,
    // curve 2 from 1 to 5, curve 3 from 4 back to 2.
    std::vector<ArrangementCurve> input;
    input.push_back(given(segment(4, 0, 5, 0), 10));
    input.push_back(given(segment(0, 0, 4, 0), 1));
    input.push_back(given(segment(1, 0, 5, 0), 2));
    input.push_back(given(segment(4, 0, 2, 0), 3));
    const Arrangement arrangement(input);
    ASSERT_EQ(arrangement.vertexCount(), 5U);
    ASSERT_EQ(arrangement.pieces().size(), 4U);
    // Pieces by curve: 0-1, 1-2 and 2-4 to curve 1; 4-5 to curve 2, a curve
    // before the link.
    std::vector<std::tuple<std::size_t, double, double>> pieces;
    for (const auto& piece : arrangement.pieces()) {
        pieces.emplace_back(piece.curve, rounded(arrangement, piece.from).x,
                            rounded(arrangement, piece.to).x);
    }
    const std::vector<std::tuple<std::size_t, double, double>> expected{
        {1, 0, 1}, {1, 1, 2}, {1, 2, 4}, {2, 4, 5}};
    EXPECT_EQ(pieces, expected);
    EXPECT_EQ(arrangement.groupCount(), 1U);
    EXPECT_EQ(boundedFaces(arrangement), 0U);
    EXPECT_TRUE(arrangement.simpleLoops(arrangement.holeCycle(0)).empty());
    // Every dart leaving the vertex at 2 runs along a single piece.
    EXPECT_EQ(arrangement.around(2).size(), 2U);
}

TEST(Arrangement, SharedStretchCreditedWithTheDirectionOfItsCurve) {
    // A triangle whose base is also covered, the other way, by curve 0.
    std::vector<ArrangementCurve> input;
    input.push_back(given(segment(3, 0, -1, 0), 0));
    input.push_back(given(segment(0, 0, 2, 0), 1));
    input.push_back(given(segment(2, 0, 1, 1), 2));
    input.push_back(given(segment(1, 1, 0, 0), 3));
    const Arrangement arrangement(input);
    EXPECT_EQ(boundedFaces(arrangement), 1U);
    EXPECT_EQ(faceSteps(arrangement, 1, 0.5), (Steps{{0, true}, {2, false}, {3, false}}));
}

TEST(Arrangement, SelfIntersectingPolyline) {
    const auto input =
        given({segment(0, 0, 2, 2), segment(2, 2, 2, 0), segment(2, 0, 0, 2), segment(0, 2, 0, 0)});
    const Arrangement arrangement(input);
    EXPECT_EQ(arrangement.vertexCount(), 5U);
    EXPECT_EQ(boundedFaces(arrangement), 2U);
    EXPECT_EQ(faceSteps(arrangement, 0.3, 1), (Steps{{0, false}, {2, false}, {3, false}}));
    EXPECT_EQ(faceSteps(arrangement, 1.7, 1), (Steps{{0, true}, {1, true}, {2, true}}));
    EXPECT_EQ(faceSteps(arrangement, 1, 0.3), std::nullopt);
    // The hole cycle passes the crossing twice: two loops.
    const auto loops = arrangement.simpleLoops(arrangement.holeCycle(0));
    ASSERT_EQ(loops.size(), 2U);
    EXPECT_EQ(loops[0].size(), 3U);
    EXPECT_EQ(loops[1].size(), 3U);
}

TEST(Arrangement, FaceWhoseBoundaryTouchesItself) {
    // A diamond inside a square, touching its bottom side at (2, 0).
    std::vector<Curve> curves = rectangle(0, 0, 4, 4);
    curves.emplace_back(segment(2, 0, 3, 1));
    curves.emplace_back(segment(3, 1, 2, 2));
    curves.emplace_back(segment(2, 2, 1, 1));
    curves.emplace_back(segment(1, 1, 2, 0));
    const auto input = given(curves);
    const Arrangement arrangement(input);
    EXPECT_EQ(arrangement.groupCount(), 1U);
    EXPECT_EQ(boundedFaces(arrangement), 2U);
    EXPECT_EQ(faceSteps(arrangement, 2, 1),
              (Steps{{4, false}, {5, false}, {6, false}, {7, false}}));

    const auto loops = arrangement.simpleLoops(boundedFaceAt(arrangement, 0.5, 3));
    ASSERT_EQ(loops.size(), 2U);
    const std::size_t outer = loops[0].size() == 5 ? 0 : 1;
    EXPECT_EQ(loops[outer].size(), 5U);
    EXPECT_EQ(loops[1 - outer].size(), 4U);
    EXPECT_TRUE(arrangement.counterclockwise(loops[outer]));
    EXPECT_FALSE(arrangement.counterclockwise(loops[1 - outer]));
    EXPECT_TRUE(arrangement.encloses(loops[outer], loops[1 - outer]));
    EXPECT_FALSE(arrangement.encloses(loops[1 - outer], loops[outer]));
    // Each loop is closed and passes every vertex once.
    for (const auto& loop : loops) {
        std::vector<std::size_t> vertices;
        for (std::size_t k = 0; k < loop.size(); ++k) {
            EXPECT_EQ(arrangement.head(loop[k]), arrangement.tail(loop[(k + 1) % loop.size()]));
            vertices.push_back(arrangement.tail(loop[k]));
        }
        std::sort(vertices.begin(), vertices.end());
        EXPECT_EQ(std::adjacent_find(vertices.begin(), vertices.end()), vertices.end());
    }
}

TEST(Arrangement, CircleWithADiameter) {
    const auto input = given({Circle{Point2{0, 0}, 2}, segment(-2, 0, 2, 0)});
    const Arrangement arrangement(input);
    EXPECT_EQ(arrangement.vertexCount(), 2U);
    EXPECT_EQ(arrangement.pieces().size(), 3U);
    EXPECT_EQ(boundedFaces(arrangement), 2U);
    EXPECT_EQ(faceSteps(arrangement, 0, 1), (Steps{{0, false}, {1, false}}));
    EXPECT_EQ(faceSteps(arrangement, 0, -1), (Steps{{0, false}, {1, true}}));
    // The island is the circle: the hole cycle has its two arcs only.
    const auto loops = arrangement.simpleLoops(arrangement.holeCycle(0));
    ASSERT_EQ(loops.size(), 1U);
    EXPECT_EQ(stepsOf(arrangement, loops[0]), (Steps{{0, true}, {0, true}}));
    EXPECT_FALSE(arrangement.counterclockwise(loops[0]));
}

TEST(Arrangement, GroupsInsideAnIsland) {
    // A square, a square island in it, a circle in the island and an open
    // segment between the two squares.
    std::vector<Curve> curves = rectangle(0, 0, 10, 10);
    for (const Curve& side : rectangle(2, 2, 8, 8)) {
        curves.push_back(side);
    }
    curves.emplace_back(Circle{Point2{5, 5}, 1});
    curves.emplace_back(segment(0.5, 0.5, 1, 1));
    const auto input = given(curves);
    const Arrangement arrangement(input);
    ASSERT_EQ(arrangement.groupCount(), 4U);
    EXPECT_EQ(boundedFaces(arrangement), 3U);
    EXPECT_EQ(faceSteps(arrangement, 1, 5),
              (Steps{{0, false}, {1, false}, {2, false}, {3, false}}));
    EXPECT_EQ(faceSteps(arrangement, 0.9, 0.5),
              (Steps{{0, false}, {1, false}, {2, false}, {3, false}}));
    EXPECT_EQ(faceSteps(arrangement, 3, 3),
              (Steps{{4, false}, {5, false}, {6, false}, {7, false}}));
    EXPECT_EQ(faceSteps(arrangement, 5, 5.5), (Steps{{8, false}}));

    const auto outer = arrangement.simpleLoops(boundedFaceAt(arrangement, 1, 5));
    ASSERT_EQ(outer.size(), 1U);
    const auto island = arrangement.simpleLoops(arrangement.holeCycle(1));
    const auto inner = arrangement.simpleLoops(arrangement.holeCycle(2));
    ASSERT_EQ(island.size(), 1U);
    ASSERT_EQ(inner.size(), 1U);
    EXPECT_TRUE(arrangement.encloses(outer[0], island[0]));
    EXPECT_TRUE(arrangement.encloses(outer[0], inner[0]));
    EXPECT_TRUE(arrangement.encloses(island[0], inner[0]));
    EXPECT_FALSE(arrangement.encloses(inner[0], island[0]));
    // The open segment has no loop: both sides of it are the same face.
    EXPECT_TRUE(arrangement.simpleLoops(arrangement.holeCycle(3)).empty());
    EXPECT_TRUE(arrangement.inside(arrangement.vertex(arrangement.groupVertex(3)), outer[0]));
}

TEST(Arrangement, MovedEndsReachAMeetingWithARadical) {
    // The upper half of the unit circle, from (1, 0) to (-1, 0), runs on
    // along its extension to the line x = -1/2, at (-1/2, -sqrt(3) / 2).
    const PolylineSegment arc{Point2{1, 0}, Point2{-1, 0}, 1.0};
    const Segment line = segment(-0.5, -2, -0.5, 2);
    ArrangementCurve moved = given(arc, 0);
    const auto meeting =
        detail::meet(detail::carrierOf(moved.meaning), detail::carrierOf(detail::meaningOf(line)));
    ASSERT_EQ(meeting.count, 2U);
    for (const auto& found : meeting.found()) {
        if (detail::compareY(found.point, at(0, 0)) == Sign::negative) {
            moved.end = found.point;
        }
    }
    ASSERT_TRUE(moved.end.has_value());
    const std::vector<ArrangementCurve> input{moved, given(line, 1)};
    const Arrangement arrangement(input);
    // (-1/2, +-sqrt(3) / 2), the ends of the line and (1, 0).
    EXPECT_EQ(arrangement.vertexCount(), 5U);
    EXPECT_EQ(boundedFaces(arrangement), 1U);
    EXPECT_EQ(faceSteps(arrangement, -0.8, 0), (Steps{{0, false}, {1, false}}));
    EXPECT_EQ(faceSteps(arrangement, 0, 0), std::nullopt);
    EXPECT_EQ(faceSteps(arrangement, -0.6, -0.85), std::nullopt);
}

// --- Properties on generated data ----------------------------------------------

// A small linear congruential generator: the same data on every system.
class Generator {
public:
    std::uint64_t next() {
        m_state = m_state * 6364136223846793005ULL + 1442695040888963407ULL;
        return m_state >> 33U;
    }

    double coordinate(std::uint64_t range) { return static_cast<double>(next() % range); }

private:
    std::uint64_t m_state = 20261114;
};

// Segments on a small integer grid (many overlaps, crossings at vertices),
// circles of integer radius and half circles of polylines.
std::vector<Curve> generated(std::size_t count) {
    Generator random;
    std::vector<Curve> curves;
    while (curves.size() < count) {
        const auto kind = random.next() % 4;
        const double x0 = random.coordinate(8);
        const double y0 = random.coordinate(8);
        const double x1 = random.coordinate(8);
        const double y1 = random.coordinate(8);
        if (kind == 0) {
            curves.emplace_back(Circle{Point2{x0, y0}, 1.0 + random.coordinate(3)});
        } else if (x0 == x1 && y0 == y1) {
            continue;
        } else if (kind == 1) {
            curves.emplace_back(PolylineSegment{Point2{x0, y0}, Point2{x1, y1}, 1.0});
        } else {
            curves.emplace_back(segment(x0, y0, x1, y1));
        }
    }
    return curves;
}

using Cycle = std::vector<std::tuple<std::size_t, bool, double, double>>;

// A cycle by the original indices of its curves and the rounded tails of its
// darts, from its smallest step.
Cycle describe(const Arrangement& arrangement, std::size_t c,
               const std::vector<std::size_t>& original) {
    Cycle steps;
    for (const std::size_t dart : arrangement.cycle(c)) {
        const Point2 from = rounded(arrangement, arrangement.tail(dart));
        steps.emplace_back(original[arrangement.pieces()[Arrangement::pieceOf(dart)].curve],
                           Arrangement::reversed(dart), from.x, from.y);
    }
    std::rotate(steps.begin(), std::min_element(steps.begin(), steps.end()), steps.end());
    return steps;
}

std::vector<Cycle> describeAll(const Arrangement& arrangement,
                               const std::vector<std::size_t>& original) {
    std::vector<Cycle> cycles;
    for (std::size_t c = 0; c < arrangement.cycleCount(); ++c) {
        cycles.push_back(describe(arrangement, c, original));
    }
    std::sort(cycles.begin(), cycles.end());
    return cycles;
}

// True when the point lies on the carrier of some curve: the tests only ask
// about points off every curve.
bool onSomeCarrier(const std::vector<Curve>& curves, const ExactPoint& point) {
    return std::any_of(curves.begin(), curves.end(), [&](const Curve& curve) {
        return detail::onCarrier(detail::carrierOf(detail::meaningOf(curve)), point);
    });
}

TEST(ArrangementProperties, PermutingTheCurvesPermutesTheOrigins) {
    const std::vector<Curve> curves = generated(24);
    std::vector<std::size_t> identity(curves.size());
    for (std::size_t i = 0; i < identity.size(); ++i) {
        identity[i] = i;
    }
    const auto input = given(curves);
    const Arrangement first(input);
    const auto expected = describeAll(first, identity);
    EXPECT_GT(boundedFaces(first), 10U);

    for (const std::size_t shift : {1U, 7U, 13U}) {
        // Curve i goes to position (i * 5 + shift) % n, keeping its priority.
        std::vector<std::size_t> original(curves.size());
        std::vector<ArrangementCurve> permuted(curves.size());
        for (std::size_t i = 0; i < curves.size(); ++i) {
            const std::size_t position = (i * 5 + shift) % curves.size();
            original[position] = i;
            permuted[position] = given(curves[i], i);
        }
        const Arrangement second(permuted);
        EXPECT_EQ(describeAll(second, original), expected) << shift;
        EXPECT_EQ(second.groupCount(), first.groupCount());
    }
}

TEST(ArrangementProperties, FacesAreCountedAndOriented) {
    const std::vector<Curve> curves = generated(24);
    const auto input = given(curves);
    const Arrangement arrangement(input);
    // Euler's formula for each group: V - E + F = 2 with its bounded faces
    // and its unbounded face, where a whole circle with its own vertex
    // counts as one vertex and one edge.
    EXPECT_EQ(arrangement.vertexCount() + boundedFaces(arrangement) + arrangement.groupCount(),
              arrangement.pieces().size() + 2 * arrangement.groupCount());
    for (std::size_t c = 0; c < arrangement.cycleCount(); ++c) {
        const auto loops = arrangement.simpleLoops(c);
        if (loops.size() == 1 && loops[0].size() == arrangement.cycle(c).size()) {
            EXPECT_EQ(arrangement.counterclockwise(loops[0]), !arrangement.isHole(c)) << c;
        }
    }
}

TEST(ArrangementProperties, FarCurvesDoNotChangeTheFaceOfAPoint) {
    std::vector<Curve> curves = generated(20);
    std::vector<ExactPoint> points;
    for (int i = 0; i < 12; ++i) {
        for (int j = 0; j < 12; ++j) {
            const ExactPoint point = at(-1.875 + 0.75 * i, -1.625 + 0.75 * j);
            if (!onSomeCarrier(curves, point)) {
                points.push_back(point);
            }
        }
    }
    ASSERT_GT(points.size(), 50U);
    std::vector<std::size_t> identity(curves.size() + 4);
    for (std::size_t i = 0; i < identity.size(); ++i) {
        identity[i] = i;
    }
    const auto input = given(curves);
    const Arrangement near(input);
    for (const Curve& side : rectangle(100, 100, 104, 103)) {
        curves.push_back(side);
    }
    const auto more = given(curves);
    const Arrangement far(more);
    for (const ExactPoint& point : points) {
        const auto before = near.faceOf(point);
        const auto after = far.faceOf(point);
        ASSERT_EQ(before.has_value(), after.has_value());
        if (before) {
            EXPECT_EQ(describe(near, *before, identity), describe(far, *after, identity));
            EXPECT_TRUE(near.inside(point, near.cycle(*before)));
        }
    }
}

TEST(ArrangementPerformance, GridOfThousandsOfSegments) {
    // A grid of 30 by 30 cells, every side of every cell its own segment:
    // 1860 segments, 961 vertices, 900 bounded faces.
    constexpr int kCells = 30;
    std::vector<Curve> curves;
    for (int i = 0; i <= kCells; ++i) {
        for (int j = 0; j < kCells; ++j) {
            curves.emplace_back(segment(j, i, j + 1, i));
            curves.emplace_back(segment(i, j, i, j + 1));
        }
    }
    const auto input = given(curves);
    const auto start = std::chrono::steady_clock::now();
    const Arrangement arrangement(input);
    const std::size_t face = boundedFaceAt(arrangement, 12.5, 17.5);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_EQ(arrangement.vertexCount(), 961U);
    EXPECT_EQ(arrangement.pieces().size(), 1860U);
    EXPECT_EQ(boundedFaces(arrangement), 900U);
    EXPECT_EQ(arrangement.cycle(face).size(), 4U);
    // Not a measure (that is the bench of PS11): only far from all pairs.
    EXPECT_LT(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count(), 20);
}

TEST(ArrangementBoxes, OverlappingPairsSkipFarBoxes) {
    const std::vector<Box> boxes{{0, 0, 1, 1}, {5, 5, 6, 6}, {1, 1, 2, 2}, {0.5, 3, 0.7, 4}};
    const std::vector<std::pair<std::size_t, std::size_t>> expected{{0, 2}};
    EXPECT_EQ(detail::overlappingPairs(boxes), expected);
}

}  // namespace
