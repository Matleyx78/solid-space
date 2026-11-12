// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Unit tests of the exact order of the pieces that leave a point (stage 0.3,
// package PS2). The data are chosen so that every expected order can be seen
// by hand: the tangent of each half piece and its curvature are written next
// to it, with the angle of the tangent from +x in degrees.

#include "local_order.hpp"

#include "carriers.hpp"
#include "curve_meaning.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <span>
#include <vector>

namespace {

using solid_space::geom2d::Circle;
using solid_space::geom2d::Curve;
using solid_space::geom2d::Point2;
using solid_space::geom2d::PolylineSegment;
using solid_space::geom2d::Segment;
using solid_space::geom2d::detail::Carrier;
using solid_space::geom2d::detail::ExactPoint;
using solid_space::geom2d::detail::HalfPiece;
using solid_space::geom2d::detail::Heading;
using solid_space::geom2d::detail::LocalOrder;
using solid_space::geom2d::detail::Placement;
using solid_space::robust::Sign;

namespace detail = solid_space::geom2d::detail;

constexpr Heading kForward = Heading::forward;
constexpr Heading kBackward = Heading::backward;

Carrier carrierOf(const Curve& curve) {
    const auto meaning = detail::meaningOf(curve);
    EXPECT_FALSE(meaning.degenerate);
    return detail::carrierOf(meaning);
}

HalfPiece half(const Curve& curve, Heading heading) {
    return HalfPiece{carrierOf(curve), heading};
}

ExactPoint at(double x, double y) {
    return detail::exactPoint(Point2{x, y});
}

// The input indices of the sorted half pieces.
std::vector<std::size_t> sortedOf(const LocalOrder& order) {
    return {order.sorted().begin(), order.sorted().end()};
}

// The groups of an order, each as its input indices.
std::vector<std::vector<std::size_t>> groupsOf(const LocalOrder& order) {
    std::vector<std::vector<std::size_t>> groups;
    for (std::size_t g = 0; g < order.groupCount(); ++g) {
        const auto members = order.group(g);
        groups.emplace_back(members.begin(), members.end());
    }
    return groups;
}

using Groups = std::vector<std::vector<std::size_t>>;

// Singleton groups, one per index.
Groups singletons(std::initializer_list<std::size_t> indices) {
    Groups groups;
    for (const std::size_t i : indices) {
        groups.push_back({i});
    }
    return groups;
}

// The groups of the pieces, sorted with the input rotated so that it starts
// at `first`, with every index mapped back to the original input and the
// members of each group in increasing order (within a group the order follows
// the input, which the rotation changes).
Groups groupsAfterRotation(const ExactPoint& point, std::span<const HalfPiece> pieces,
                           std::size_t first) {
    std::vector<HalfPiece> rotated;
    for (std::size_t k = 0; k < pieces.size(); ++k) {
        rotated.push_back(pieces[(first + k) % pieces.size()]);
    }
    const LocalOrder order(point, rotated);
    Groups groups;
    for (std::size_t g = 0; g < order.groupCount(); ++g) {
        std::vector<std::size_t> members;
        for (const std::size_t i : order.group(g)) {
            members.push_back((first + i) % pieces.size());
        }
        std::sort(members.begin(), members.end());
        groups.push_back(members);
    }
    return groups;
}

// Lines and circles mixed, at the origin, all with different tangents.
std::vector<HalfPiece> mixedAtOrigin() {
    const Curve xAxis = Segment{{-1.0, 0.0}, {1.0, 0.0}};
    const Curve diagonal = Segment{{0.0, 0.0}, {2.0, 2.0}};
    const Curve yAxis = Segment{{0.0, -1.0}, {0.0, 1.0}};
    // Centre (3, 4), radius 5, through the origin: P - C = (-3, -4), so the
    // tangent counterclockwise is (4, -3) (323.13 degrees) and clockwise
    // (-4, 3) (143.13 degrees).
    const Curve circle = Circle{{3.0, 4.0}, 5.0};
    return {
        half(xAxis, kForward),      // 0: (1, 0), 0 degrees
        half(diagonal, kForward),   // 1: (1, 1), 45
        half(yAxis, kForward),      // 2: (0, 1), 90
        half(circle, kBackward),    // 3: (-4, 3), 143.13
        half(xAxis, kBackward),     // 4: (-1, 0), 180
        half(diagonal, kBackward),  // 5: (-1, -1), 225
        half(yAxis, kBackward),     // 6: (0, -1), 270
        half(circle, kForward),     // 7: (4, -3), 323.13
    };
}

TEST(LocalOrder, sortsLinesAndCirclesByTheirTangent) {
    const auto pieces = mixedAtOrigin();
    const ExactPoint origin = at(0.0, 0.0);
    // Shuffled input: the order comes from the geometry only.
    const std::vector<std::size_t> shuffle{5, 2, 7, 0, 3, 6, 1, 4};
    std::vector<HalfPiece> input;
    input.reserve(shuffle.size());
    for (const std::size_t i : shuffle) {
        input.push_back(pieces[i]);
    }
    const LocalOrder order(origin, input);
    ASSERT_EQ(order.size(), 8U);
    ASSERT_EQ(order.groupCount(), 8U);
    std::vector<std::size_t> sorted;
    for (const std::size_t i : order.sorted()) {
        sorted.push_back(shuffle[i]);
    }
    EXPECT_EQ(sorted, (std::vector<std::size_t>{0, 1, 2, 3, 4, 5, 6, 7}));
    for (std::size_t g = 0; g < order.groupCount(); ++g) {
        ASSERT_EQ(order.group(g).size(), 1U);
        EXPECT_EQ(order.groupOf(order.group(g)[0]), g);
    }
}

TEST(LocalOrder, doesNotDependOnTheFirstPiece) {
    const auto pieces = mixedAtOrigin();
    const ExactPoint origin = at(0.0, 0.0);
    const Groups expected = singletons({0, 1, 2, 3, 4, 5, 6, 7});
    for (std::size_t first = 0; first < pieces.size(); ++first) {
        EXPECT_EQ(groupsAfterRotation(origin, pieces, first), expected) << "first " << first;
    }
}

TEST(LocalOrder, comparesEveryPairBothWays) {
    const auto pieces = mixedAtOrigin();
    const ExactPoint origin = at(0.0, 0.0);
    for (std::size_t i = 0; i < pieces.size(); ++i) {
        for (std::size_t j = 0; j < pieces.size(); ++j) {
            Sign expected = Sign::zero;
            if (i != j) {
                expected = i < j ? Sign::negative : Sign::positive;
            }
            EXPECT_EQ(detail::compareAround(origin, pieces[i], pieces[j]), expected)
                << i << " " << j;
        }
    }
}

// Circles tangent to the x axis at the origin, inside and outside each other,
// and the x axis itself. At the origin:
// - circle (0, 1), r 1: P - C = (0, -1), counterclockwise tangent (1, 0) with
//   curvature +1, clockwise (-1, 0) with -1;
// - circle (0, 2), r 2: counterclockwise (1, 0) with +1/2, clockwise (-1, 0)
//   with -1/2 (tangent inside the first one);
// - circle (0, -1), r 1: P - C = (0, 1), counterclockwise (-1, 0) with +1,
//   clockwise (1, 0) with -1 (tangent outside the first one);
// - the x axis: (1, 0) and (-1, 0) with curvature 0.
// At equal tangent the larger signed curvature turns left sooner, so it comes
// later counterclockwise.
std::vector<HalfPiece> tangentAtOrigin() {
    const Curve small = Circle{{0.0, 1.0}, 1.0};
    const Curve large = Circle{{0.0, 2.0}, 2.0};
    const Curve below = Circle{{0.0, -1.0}, 1.0};
    const Curve xAxis = Segment{{-3.0, 0.0}, {3.0, 0.0}};
    return {
        half(below, kBackward),  // 0: (1, 0), -1
        half(xAxis, kForward),   // 1: (1, 0), 0
        half(large, kForward),   // 2: (1, 0), +1/2
        half(small, kForward),   // 3: (1, 0), +1
        half(small, kBackward),  // 4: (-1, 0), -1
        half(large, kBackward),  // 5: (-1, 0), -1/2
        half(xAxis, kBackward),  // 6: (-1, 0), 0
        half(below, kForward),   // 7: (-1, 0), +1
    };
}

TEST(LocalOrder, sortsEqualTangentsByCurvature) {
    const auto pieces = tangentAtOrigin();
    const ExactPoint origin = at(0.0, 0.0);
    const Groups expected = singletons({0, 1, 2, 3, 4, 5, 6, 7});
    for (std::size_t first = 0; first < pieces.size(); ++first) {
        EXPECT_EQ(groupsAfterRotation(origin, pieces, first), expected) << "first " << first;
    }
    const LocalOrder order(origin, pieces);
    EXPECT_EQ(order.groupCount(), 8U);
}

TEST(LocalOrder, separatesEqualCurvaturesOfOppositeSign) {
    const auto pieces = tangentAtOrigin();
    const ExactPoint origin = at(0.0, 0.0);
    // Same tangent (1, 0), curvatures -1 and +1: the circle below comes first.
    EXPECT_EQ(detail::compareAround(origin, pieces[0], pieces[3]), Sign::negative);
    EXPECT_EQ(detail::compareAround(origin, pieces[3], pieces[0]), Sign::positive);
    // Same tangent (-1, 0), curvatures -1 and +1.
    EXPECT_EQ(detail::compareAround(origin, pieces[4], pieces[7]), Sign::negative);
    // Same curvature +1, opposite tangents: 0 degrees before 180.
    EXPECT_EQ(detail::compareAround(origin, pieces[3], pieces[7]), Sign::negative);
}

TEST(LocalOrder, groupsTheSameCarrierInTheSameHeading) {
    const ExactPoint origin = at(0.0, 0.0);
    // Three lines along the x axis toward +x, written differently: the
    // backward heading of a segment from (2, 0) to (-1, 0) runs toward +x.
    const Curve shortLine = Segment{{0.0, 0.0}, {1.0, 0.0}};
    const Curve longLine = Segment{{-5.0, 0.0}, {3.0, 0.0}};
    const Curve reversedLine = Segment{{2.0, 0.0}, {-1.0, 0.0}};
    // The circle of centre (0, 1) and radius 1, as a Circle and as two
    // polyline arcs with weight 4: from (0, 0) to (0, 2) with bulge 1
    // (counterclockwise) and from (0, 2) to (0, 0) with bulge -1 (clockwise),
    // both with centre (0, 1) and radius 1.
    const Curve circle = Circle{{0.0, 1.0}, 1.0};
    const Curve arcUp = PolylineSegment{{0.0, 0.0}, {0.0, 2.0}, 1.0};
    const Curve arcDown = PolylineSegment{{0.0, 2.0}, {0.0, 0.0}, -1.0};
    const std::vector<HalfPiece> pieces{
        half(circle, kForward),         // 0: (1, 0), +1
        half(shortLine, kForward),      // 1: (1, 0), 0
        half(arcUp, kForward),          // 2: (1, 0), +1
        half(longLine, kBackward),      // 3: (-1, 0), 0
        half(reversedLine, kBackward),  // 4: (1, 0), 0
        half(arcDown, kForward),        // 5: (1, 0), +1
        half(longLine, kForward),       // 6: (1, 0), 0
    };
    const LocalOrder order(origin, pieces);
    const std::vector<std::vector<std::size_t>> expected{{1, 4, 6}, {0, 2, 5}, {3}};
    EXPECT_EQ(groupsOf(order), expected);
    EXPECT_EQ(sortedOf(order), (std::vector<std::size_t>{1, 4, 6, 0, 2, 5, 3}));
    EXPECT_EQ(order.groupOf(4), 0U);
    EXPECT_EQ(order.groupOf(5), 1U);
    EXPECT_EQ(order.groupOf(3), 2U);
    EXPECT_EQ(detail::compareAround(origin, pieces[0], pieces[5]), Sign::zero);
    EXPECT_EQ(detail::compareAround(origin, pieces[1], pieces[4]), Sign::zero);
}

// The unit circle meets the line y = x at P = (sqrt(2) / 2, sqrt(2) / 2). The
// circle of centre (2, -2) and radius 3 passes through P too:
// (sqrt(2)/2 - 2)^2 + (sqrt(2)/2 + 2)^2 = 2 (1/2 + 4) = 9. At P:
// - the line: (1, 1) at 45 degrees and (-1, -1) at 225;
// - the unit circle: P - C = P, counterclockwise (-1, 1) at 135, clockwise
//   (1, -1) at 315;
// - the circle of radius 3: P - C = (-1.29, 2.71), counterclockwise
//   (-2.71, -1.29) at 205.5, clockwise (2.71, 1.29) at 25.5.
// The upper half of the unit circle as a polyline arc from (1, 0) to (-1, 0)
// with bulge 1 has the same circle, with weight 4.
struct RootCase {
    std::vector<HalfPiece> pieces;
    Groups expected;
};

RootCase rootCase() {
    const Curve diagonal = Segment{{0.0, 0.0}, {1.0, 1.0}};
    const Curve unit = Circle{{0.0, 0.0}, 1.0};
    const Curve wide = Circle{{2.0, -2.0}, 3.0};
    const Curve upperHalf = PolylineSegment{{1.0, 0.0}, {-1.0, 0.0}, 1.0};
    return RootCase{{
                        half(diagonal, kForward),   // 0: 45
                        half(diagonal, kBackward),  // 1: 225
                        half(unit, kForward),       // 2: 135
                        half(unit, kBackward),      // 3: 315
                        half(wide, kForward),       // 4: 205.5
                        half(wide, kBackward),      // 5: 25.5
                        half(upperHalf, kForward),  // 6: 135, as 2
                    },
                    {{5}, {0}, {2, 6}, {4}, {1}, {3}}};
}

ExactPoint upperMeeting(const Curve& first, const Curve& second) {
    const auto meeting = detail::meet(carrierOf(first), carrierOf(second));
    EXPECT_EQ(meeting.count, 2U);
    for (const auto& point : meeting.found()) {
        if (detail::compareX(point.point, at(0.0, 0.0)) == Sign::positive) {
            return point.point;
        }
    }
    ADD_FAILURE() << "no meeting with x > 0";
    return at(0.0, 0.0);
}

TEST(LocalOrder, sortsAroundAPointWithARoot) {
    const auto test = rootCase();
    const ExactPoint point = upperMeeting(Segment{{0.0, 0.0}, {1.0, 1.0}}, Circle{{0.0, 0.0}, 1.0});
    ASSERT_FALSE(point.radicand.isZero());
    for (std::size_t first = 0; first < test.pieces.size(); ++first) {
        EXPECT_EQ(groupsAfterRotation(point, test.pieces, first), test.expected)
            << "first " << first;
    }
    const LocalOrder order(point, test.pieces);
    EXPECT_EQ(order.groupCount(), 6U);
    EXPECT_EQ(order.groupOf(2), order.groupOf(6));
}

TEST(LocalOrder, givesTheSameOrderForEveryRepresentationOfThePoint) {
    const auto test = rootCase();
    // The same point, from the meeting of a line and a circle and from the
    // meeting of two circles: two different radicands.
    const ExactPoint fromLine =
        upperMeeting(Segment{{0.0, 0.0}, {1.0, 1.0}}, Circle{{0.0, 0.0}, 1.0});
    const ExactPoint fromCircles = upperMeeting(Circle{{0.0, 0.0}, 1.0}, Circle{{2.0, -2.0}, 3.0});
    ASSERT_TRUE(detail::samePoint(fromLine, fromCircles));
    ASSERT_NE(compare(fromLine.radicand, fromCircles.radicand), Sign::zero);
    EXPECT_EQ(groupsOf(LocalOrder(fromCircles, test.pieces)), test.expected);
    EXPECT_EQ(groupsOf(LocalOrder(fromLine, test.pieces)), test.expected);
}

TEST(LocalOrder, sortsAroundPointsWithDifferentRoots) {
    // The unit circle meets the line through the origin with direction (1, 2)
    // at P = (1, 2) / sqrt(5): radicand 5, not 2. Tangents at P: the line
    // (1, 2) at 63.4 degrees and (-1, -2) at 243.4; the unit circle (-2, 1)
    // at 153.4 counterclockwise and (2, -1) at 333.4 clockwise.
    const Curve steep = Segment{{0.0, 0.0}, {1.0, 2.0}};
    const Curve unit = Circle{{0.0, 0.0}, 1.0};
    const ExactPoint point = upperMeeting(steep, unit);
    const std::vector<HalfPiece> pieces{
        half(unit, kBackward),   // 0: 333.4
        half(steep, kBackward),  // 1: 243.4
        half(unit, kForward),    // 2: 153.4
        half(steep, kForward),   // 3: 63.4
    };
    for (std::size_t first = 0; first < pieces.size(); ++first) {
        EXPECT_EQ(groupsAfterRotation(point, pieces, first), singletons({3, 2, 1, 0}))
            << "first " << first;
    }
}

TEST(LocalOrder, placesAReferenceAlongAGroupOrInASector) {
    const auto pieces = mixedAtOrigin();
    const ExactPoint origin = at(0.0, 0.0);
    const LocalOrder order(origin, pieces);
    const auto expectPlacement = [&order](const HalfPiece& reference, bool along,
                                          std::size_t index) {
        const Placement placement = order.place(reference);
        EXPECT_EQ(placement.along, along);
        EXPECT_EQ(placement.index, index);
    };
    // Along the diagonal (group 1), written as another segment.
    expectPlacement(half(Segment{{0.0, 0.0}, {3.0, 3.0}}, kForward), true, 1);
    // (-1, 1), 135 degrees: between the y axis (90) and the circle (143.13).
    expectPlacement(half(Segment{{0.0, 0.0}, {-1.0, 1.0}}, kForward), false, 2);
    // (1, -1), 315 degrees: between the y axis down (270) and the circle
    // (323.13).
    expectPlacement(half(Segment{{0.0, 0.0}, {1.0, -1.0}}, kForward), false, 6);
    // (10, -1), 354.3 degrees: after the last group, in the sector that
    // closes the turn.
    expectPlacement(half(Segment{{0.0, 0.0}, {10.0, -1.0}}, kForward), false, 7);
    // Tangent +x with negative curvature: just below the +x axis, so in the
    // closing sector too, although its tangent is the one of group 0.
    expectPlacement(half(Circle{{0.0, -1.0}, 1.0}, kBackward), false, 7);
    // Tangent +x with positive curvature: just above group 0.
    expectPlacement(half(Circle{{0.0, 1.0}, 1.0}, kForward), false, 0);
    // The half-line toward +x lies along the x axis.
    const Placement ray = order.place(detail::rayDeparture(origin));
    EXPECT_TRUE(ray.along);
    EXPECT_EQ(ray.index, 0U);
}

TEST(LocalOrder, placesTheRayAmongTangentPieces) {
    const ExactPoint origin = at(0.0, 0.0);
    const std::vector<HalfPiece> pieces{
        half(Circle{{0.0, 1.0}, 1.0}, kForward),    // (1, 0), +1
        half(Circle{{0.0, -1.0}, 1.0}, kBackward),  // (1, 0), -1
    };
    const LocalOrder order(origin, pieces);
    ASSERT_EQ(order.groupCount(), 2U);
    // The ray (curvature 0) lies between them, in sector 0.
    const Placement ray = order.place(detail::rayDeparture(origin));
    EXPECT_FALSE(ray.along);
    EXPECT_EQ(ray.index, 0U);
}

TEST(LocalOrder, placesEverythingInTheWholeTurnWithNoPieces) {
    const ExactPoint origin = at(1.0, 2.0);
    const LocalOrder order(origin, {});
    EXPECT_EQ(order.size(), 0U);
    EXPECT_EQ(order.groupCount(), 0U);
    const Placement placement = order.place(detail::rayDeparture(origin));
    EXPECT_FALSE(placement.along);
    EXPECT_EQ(placement.index, 0U);
}

TEST(LocalOrder, sortsTheTangentsOfEveryQuarter) {
    // Lines through (1, 1) in sixteen directions, given in reverse order:
    // the axes, the diagonals and the directions in between, two per quarter
    // of a half turn, so that every pair of the half-plane test is used.
    const std::vector<Point2> directions{
        {1.0, 0.0},  {2.0, 1.0},  {1.0, 1.0},  {1.0, 2.0},   {0.0, 1.0},   {-1.0, 2.0},
        {-1.0, 1.0}, {-2.0, 1.0}, {-1.0, 0.0}, {-2.0, -1.0}, {-1.0, -1.0}, {-1.0, -2.0},
        {0.0, -1.0}, {1.0, -2.0}, {1.0, -1.0}, {2.0, -1.0},
    };
    const ExactPoint point = at(1.0, 1.0);
    const std::vector<Point2> reversedDirections(directions.rbegin(), directions.rend());
    std::vector<HalfPiece> pieces;
    pieces.reserve(reversedDirections.size());
    for (const auto& direction : reversedDirections) {
        pieces.push_back(
            half(Segment{{1.0, 1.0}, {1.0 + direction.x, 1.0 + direction.y}}, kForward));
    }
    const LocalOrder order(point, pieces);
    ASSERT_EQ(order.groupCount(), directions.size());
    for (std::size_t k = 0; k < directions.size(); ++k) {
        EXPECT_EQ(order.sorted()[k], directions.size() - 1 - k) << k;
    }
}

}  // namespace
