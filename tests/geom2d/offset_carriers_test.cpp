// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Unit tests of the offset carriers and their exact meetings (stage 0.2,
// package P12). The expected points are worked out by hand in the comments and
// written as exact numbers of the tower, compared with the points found by
// exact sign decisions; their doubles were computed apart with Python's decimal
// module at 60 digits. Every point found is also checked by substitution in the
// equations of both carriers, with the arithmetic of the tower.

#include "offset_carriers.hpp"

#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "exact/tower.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <variant>
#include <vector>

namespace {

using solid_space::geom2d::Arc;
using solid_space::geom2d::Circle;
using solid_space::geom2d::Contact;
using solid_space::geom2d::Curve;
using solid_space::geom2d::Point2;
using solid_space::geom2d::PolylineSegment;
using solid_space::geom2d::Segment;
using solid_space::geom2d::detail::ArcMeaning;
using solid_space::geom2d::detail::CurveMeaning;
using solid_space::geom2d::detail::Dyadic;
using solid_space::geom2d::detail::OffsetCarrier;
using solid_space::geom2d::detail::OffsetMeeting;
using solid_space::geom2d::detail::OffsetStatus;
using solid_space::geom2d::detail::TowerPoint;
using solid_space::geom2d::exact::kMaxTowerRoots;
using solid_space::geom2d::exact::Tower;
using solid_space::geom2d::exact::TowerNumber;
using solid_space::robust::Sign;

namespace detail = solid_space::geom2d::detail;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

double below(double value) {
    return std::nextafter(value, -kInfinity);
}

double above(double value) {
    return std::nextafter(value, kInfinity);
}

CurveMeaning meaningOf(const Curve& curve) {
    auto meaning = detail::meaningOf(curve);
    EXPECT_FALSE(meaning.degenerate);
    return meaning;
}

OffsetCarrier offset(Tower& tower, const Curve& curve, double distance) {
    const auto result = detail::offsetCarrier(tower, meaningOf(curve), distance);
    EXPECT_EQ(result.status, OffsetStatus::done);
    return result.carrier;
}

TowerNumber number(double value) {
    return TowerNumber(Dyadic(value));
}

TowerNumber rootOf(Tower& tower, double radicand) {
    const auto root = tower.squareRoot(number(radicand));
    EXPECT_TRUE(root.has_value());
    return root.value_or(TowerNumber());
}

TowerPoint point(TowerNumber x, TowerNumber y, double weight) {
    return TowerPoint{std::move(x), std::move(y), Dyadic(weight)};
}

// A point of carriers.hpp in the tower: its root must fit.
TowerPoint converted(Tower& tower, const detail::ExactPoint& exactPoint) {
    const auto result = detail::towerPoint(tower, exactPoint);
    EXPECT_TRUE(result.has_value());
    return result.value_or(TowerPoint{});
}

void expectWithinUlps(double actual, double expected, int ulps) {
    const double ulp = above(std::abs(expected)) - std::abs(expected);
    EXPECT_LE(std::abs(actual - expected), ulps * ulp) << actual << " vs " << expected;
}

void expectRounded(const Tower& tower, const TowerPoint& found, Point2 expected) {
    const auto rounded = detail::roundedPoint(tower, found);
    expectWithinUlps(rounded.x, expected.x, 2);
    expectWithinUlps(rounded.y, expected.y, 2);
}

// The meeting of a and b, checked on the way: every point lies exactly on both
// carriers, and the swapped call gives the same points, in the same order,
// with the same representation.
OffsetMeeting meetChecked(Tower& tower, const OffsetCarrier& a, const OffsetCarrier& b) {
    const auto meeting = detail::meetOffset(tower, a, b);
    EXPECT_TRUE(meeting.has_value());
    if (!meeting) {
        return {};
    }
    const auto swapped = detail::meetOffset(tower, b, a);
    EXPECT_TRUE(swapped.has_value());
    if (!swapped) {
        return *meeting;
    }
    EXPECT_EQ(meeting->sameCarrier, swapped->sameCarrier);
    EXPECT_EQ(meeting->count, swapped->count);
    const auto found = meeting->found();
    const auto foundSwapped = swapped->found();
    for (std::size_t i = 0; i < found.size() && i < foundSwapped.size(); ++i) {
        const auto& p = found[i];
        const auto& q = foundSwapped[i];
        EXPECT_TRUE(detail::onCarrier(tower, a, p.point)) << i;
        EXPECT_TRUE(detail::onCarrier(tower, b, p.point)) << i;
        EXPECT_EQ(detail::sideSign(tower, a, p.point), Sign::zero);
        EXPECT_EQ(detail::sideSign(tower, b, p.point), Sign::zero);
        EXPECT_EQ(p.contact, q.contact);
        EXPECT_TRUE(sameRepresentation(p.point.x, q.point.x)) << i;
        EXPECT_TRUE(sameRepresentation(p.point.y, q.point.y)) << i;
        EXPECT_EQ(compare(p.point.weight, q.point.weight), Sign::zero) << i;
        const auto rounded = detail::roundedPoint(tower, p.point);
        const auto roundedSwapped = detail::roundedPoint(tower, q.point);
        EXPECT_EQ(rounded.x, roundedSwapped.x);
        EXPECT_EQ(rounded.y, roundedSwapped.y);
    }
    if (meeting->sameCarrier) {
        EXPECT_EQ(meeting->count, 0U);
    }
    return *meeting;
}

// The curved polyline pieces of several tests. A runs counterclockwise from
// (0, 0) to (1, 1) with bulge 1/2: centre (1, 1)/2 + ((1/4 - 1) / 2) (1, -1)
// = (1/8, 7/8) and radius^2 = 1/64 + 49/64 = 25/32, so R = 5 sqrt(2) / 8. B is
// its mirror in the x axis, clockwise: centre (1/8, -7/8), the same R.
const PolylineSegment kPieceA{{0.0, 0.0}, {1.0, 1.0}, 0.5};
const PolylineSegment kPieceB{{0.0, 0.0}, {1.0, -1.0}, -0.5};
// R to 40 digits, and the double nearest to it, which lies above it.
constexpr double kRadiusOfA = 0.88388347648318440550105545263106129910604;

// The line of (0, 0)-(1, 2) moved by 1 to its left is -2x + y = sqrt(5), that
// is y = 2x + sqrt(5); the circle of radius 3 about the origin moved by 1 (it
// runs counterclockwise, so inward) has radius 2. Then
// 5x^2 + 4 sqrt(5) x + 1 = 0, x = (-2 sqrt(5) -+ sqrt(15)) / 5 and
// y = 2x + sqrt(5) = (sqrt(5) -+ 2 sqrt(15)) / 5: the smaller x first, along
// (1, 2).
TEST(OffsetCarriers, SlantedSegmentAgainstShiftedCircle) {
    Tower tower;
    const auto line = offset(tower, Segment{{0.0, 0.0}, {1.0, 2.0}}, 1.0);
    const auto circle = offset(tower, Circle{{0.0, 0.0}, 3.0}, 1.0);
    const auto meeting = meetChecked(tower, line, circle);
    ASSERT_EQ(meeting.count, 2U);
    EXPECT_FALSE(meeting.sameCarrier);
    const auto s5 = rootOf(tower, 5.0);
    const auto s15 = rootOf(tower, 15.0);
    const auto first = point(-s5.timesPowerOfTwo(1) - s15, s5 - s15.timesPowerOfTwo(1), 5.0);
    const auto second = point(s15 - s5.timesPowerOfTwo(1), s5 + s15.timesPowerOfTwo(1), 5.0);
    EXPECT_TRUE(detail::samePoint(tower, meeting.points[0].point, first));
    EXPECT_TRUE(detail::samePoint(tower, meeting.points[1].point, second));
    EXPECT_FALSE(detail::samePoint(tower, meeting.points[0].point, second));
    EXPECT_EQ(meeting.points[0].contact, Contact::crossing);
    EXPECT_EQ(meeting.points[1].contact, Contact::crossing);
    expectRounded(tower, meeting.points[0].point, {-1.6690238602413991, -1.1019797429830087});
    expectRounded(tower, meeting.points[1].point, {-0.1198305217584325, 1.9964069339829247});
}

// The same line against piece A moved inward by 1/8: radius (5 sqrt(2) - 1) / 8
// about (1/8, 7/8), nested roots. The doubles of the two points solve
// (x - 1/8)^2 + (2x + sqrt(5) - 7/8)^2 = ((5 sqrt(2) - 1) / 8)^2, worked out
// with decimal.
TEST(OffsetCarriers, SlantedSegmentAgainstShiftedCurvedPiece) {
    Tower tower;
    const auto line = offset(tower, Segment{{0.0, 0.0}, {1.0, 2.0}}, 1.0);
    const auto arc = offset(tower, kPieceA, 0.125);
    const auto meeting = meetChecked(tower, line, arc);
    ASSERT_EQ(meeting.count, 2U);
    expectRounded(tower, meeting.points[0].point, {-0.6260068562781962, 0.9840542649433973});
    expectRounded(tower, meeting.points[1].point, {-0.4128475257216356, 1.4103729260565185});
    // On the offset circle means at the new radius from the old centre.
    const auto centre = detail::towerPoint(Point2{0.125, 0.875});
    const auto s2 = rootOf(tower, 2.0);
    const auto radius = point(s2 * Dyadic(5.0) - number(1.0), TowerNumber(), 8.0);
    const auto onRadius = point(number(0.125) + radius.x * Dyadic(0.125), number(0.875), 1.0);
    for (const auto& found : meeting.found()) {
        EXPECT_EQ(detail::compareDistances(tower, centre, found.point, onRadius), Sign::zero);
    }
}

// Pieces A and B moved by 1/8: A (counterclockwise) shrinks to R - 1/8, B
// (clockwise, left is outside) grows to R + 1/8. With centres (1/8, +-7/8),
// (y - 7/8)^2 - (y + 7/8)^2 = (R - 1/8)^2 - (R + 1/8)^2 gives
// y = (R / 8) / (7 / 8) = 5 sqrt(2) / 56, and then
// (x - 1/8)^2 = (51 - 10 sqrt(2)) / 64 - (2451 - 490 sqrt(2)) / 3136 = 3 / 196,
// x = 1/8 +- sqrt(3) / 14 = (7 +- 4 sqrt(3)) / 56. B comes first in the order
// of the data (its centre is lower), and the line from its centre to the
// centre of A points up: the point on its right, x = 1/8 + sqrt(3) / 14,
// comes first.
TEST(OffsetCarriers, TwoShiftedCurvedPieces) {
    Tower tower;
    const auto a = offset(tower, kPieceA, 0.125);
    const auto b = offset(tower, kPieceB, 0.125);
    // Both radii are sqrt(3.125) / 2: one root for both.
    EXPECT_EQ(tower.size(), 1U);
    const auto meeting = meetChecked(tower, a, b);
    ASSERT_EQ(meeting.count, 2U);
    const auto s2 = rootOf(tower, 2.0);
    const auto s3 = rootOf(tower, 3.0);
    const auto y = s2 * Dyadic(5.0);
    EXPECT_TRUE(detail::samePoint(tower, meeting.points[0].point,
                                  point(number(7.0) + s3 * Dyadic(4.0), y, 56.0)));
    EXPECT_TRUE(detail::samePoint(tower, meeting.points[1].point,
                                  point(number(7.0) - s3 * Dyadic(4.0), y, 56.0)));
    expectRounded(tower, meeting.points[0].point, {0.2487179148263484, 0.12626906806902635});
    expectRounded(tower, meeting.points[1].point, {0.0012820851736516218, 0.12626906806902635});
}

// The line of (0, 0)-(1, 2) moved by d lies at distance |d| from the origin;
// the circle of radius 3 moved inward by 2 has radius 1. d = 1: tangent at
// (-2, 1) / sqrt(5) = (-2 sqrt(5), sqrt(5)) / 5; d = -1: at the opposite
// point. One ulp farther, no point; one ulp nearer, two crossings.
TEST(OffsetCarriers, ShiftedLineTangentToShiftedCircle) {
    Tower tower;
    const auto circle = offset(tower, Circle{{0.0, 0.0}, 3.0}, 2.0);
    const Segment segment{{0.0, 0.0}, {1.0, 2.0}};
    const auto s5 = rootOf(tower, 5.0);
    for (const double side : {1.0, -1.0}) {
        SCOPED_TRACE(side);
        const auto tangent = meetChecked(tower, offset(tower, segment, side), circle);
        ASSERT_EQ(tangent.count, 1U);
        EXPECT_EQ(tangent.points[0].contact, Contact::touching);
        const auto expected = point(-s5 * Dyadic(2.0 * side), s5 * Dyadic(side), 5.0);
        EXPECT_TRUE(detail::samePoint(tower, tangent.points[0].point, expected));
        expectRounded(tower, tangent.points[0].point,
                      {-0.8944271909999159 * side, 0.4472135954999579 * side});
        const double farther = side > 0.0 ? above(side) : below(side);
        const double nearer = side > 0.0 ? below(side) : above(side);
        EXPECT_EQ(meetChecked(tower, offset(tower, segment, farther), circle).count, 0U);
        const auto crossing = meetChecked(tower, offset(tower, segment, nearer), circle);
        ASSERT_EQ(crossing.count, 2U);
        EXPECT_EQ(crossing.points[0].contact, Contact::crossing);
    }
}

// One carrier moved and the other not. The unit circle meets the line of
// (0, 0)-(1, 2) moved by 1 (distance 1 from the origin) only at
// (-2 sqrt(5), sqrt(5)) / 5; the line y = 1 touches the circle of radius 3
// moved inward by 2 at (0, 1). One ulp either way gives no point or two. The
// line of (0, 0)-(3, 4) moved by 5 is the line through (-4, 3) and (-1, 7);
// the circle of radius 3 moved by 1 is the circle of radius 2.
TEST(OffsetCarriers, ShiftedAgainstUnshifted) {
    Tower tower;
    const Segment slanted{{0.0, 0.0}, {1.0, 2.0}};
    const auto unit = offset(tower, Circle{{0.0, 0.0}, 1.0}, 0.0);
    const auto tangent = meetChecked(tower, offset(tower, slanted, 1.0), unit);
    ASSERT_EQ(tangent.count, 1U);
    EXPECT_EQ(tangent.points[0].contact, Contact::touching);
    const auto s5 = rootOf(tower, 5.0);
    EXPECT_TRUE(
        detail::samePoint(tower, tangent.points[0].point, point(-s5.timesPowerOfTwo(1), s5, 5.0)));
    EXPECT_EQ(meetChecked(tower, offset(tower, slanted, above(1.0)), unit).count, 0U);
    EXPECT_EQ(meetChecked(tower, offset(tower, slanted, below(1.0)), unit).count, 2U);

    const auto level = offset(tower, Segment{{0.0, 1.0}, {1.0, 1.0}}, 0.0);
    const Circle circle{{0.0, 0.0}, 3.0};
    const auto touching = meetChecked(tower, level, offset(tower, circle, 2.0));
    ASSERT_EQ(touching.count, 1U);
    EXPECT_EQ(touching.points[0].contact, Contact::touching);
    EXPECT_TRUE(
        detail::samePoint(tower, touching.points[0].point, detail::towerPoint(Point2{0.0, 1.0})));
    EXPECT_EQ(meetChecked(tower, level, offset(tower, circle, above(2.0))).count, 0U);
    EXPECT_EQ(meetChecked(tower, level, offset(tower, circle, below(2.0))).count, 2U);

    EXPECT_TRUE(meetChecked(tower, offset(tower, Segment{{0.0, 0.0}, {3.0, 4.0}}, 5.0),
                            offset(tower, Segment{{-4.0, 3.0}, {-1.0, 7.0}}, 0.0))
                    .sameCarrier);
    EXPECT_TRUE(
        meetChecked(tower, offset(tower, circle, 1.0), offset(tower, Circle{{0.0, 0.0}, 2.0}, 0.0))
            .sameCarrier);
    // Unshifted and shifted lines that cross, and a line through a shifted
    // circle.
    EXPECT_EQ(meetChecked(tower, level, offset(tower, slanted, 1.0)).count, 1U);
    EXPECT_EQ(meetChecked(tower, offset(tower, slanted, 0.0), offset(tower, circle, 1.0)).count,
              2U);
}

// Pieces A and B moved by 1/2 and 5/4: radii R - 1/2 and R + 5/4, whose
// difference 7/4 is the distance of the centres. B holds A and touches it at
// the top of A: (1/8, 7/8 + R - 1/2) = (1, 3 + 5 sqrt(2)) / 8. One ulp more on
// B, A lies strictly inside; one ulp less, they cross.
TEST(OffsetCarriers, ShiftedCurvedPiecesTangentInside) {
    Tower tower;
    const auto a = offset(tower, kPieceA, 0.5);
    const auto tangent = meetChecked(tower, a, offset(tower, kPieceB, 1.25));
    ASSERT_EQ(tangent.count, 1U);
    EXPECT_EQ(tangent.points[0].contact, Contact::touching);
    const auto s2 = rootOf(tower, 2.0);
    EXPECT_TRUE(detail::samePoint(tower, tangent.points[0].point,
                                  point(number(1.0), number(3.0) + s2 * Dyadic(5.0), 8.0)));
    expectRounded(tower, tangent.points[0].point, {0.125, 1.2588834764831844});
    EXPECT_EQ(meetChecked(tower, a, offset(tower, kPieceB, above(1.25))).count, 0U);
    const auto crossing = meetChecked(tower, a, offset(tower, kPieceB, below(1.25)));
    ASSERT_EQ(crossing.count, 2U);
    EXPECT_EQ(crossing.points[1].contact, Contact::crossing);
}

// Circles of radius 2 about (0, 0) and (5, 0), both moved outward by 1/2: they
// touch at (5/2, 0).
TEST(OffsetCarriers, ShiftedCirclesTangentOutside) {
    Tower tower;
    const Circle left{{0.0, 0.0}, 2.0};
    const Circle right{{5.0, 0.0}, 2.0};
    const auto tangent = meetChecked(tower, offset(tower, left, -0.5), offset(tower, right, -0.5));
    ASSERT_EQ(tangent.count, 1U);
    EXPECT_EQ(tangent.points[0].contact, Contact::touching);
    EXPECT_TRUE(
        detail::samePoint(tower, tangent.points[0].point, detail::towerPoint(Point2{2.5, 0.0})));
    EXPECT_EQ(
        meetChecked(tower, offset(tower, left, -0.5), offset(tower, right, above(-0.5))).count, 0U);
    EXPECT_EQ(
        meetChecked(tower, offset(tower, left, below(-0.5)), offset(tower, right, -0.5)).count, 2U);
}

// y = 2x + sqrt(5) against the line of (0, 0)-(1, 0) moved up by 1, y = 1:
// x = (1 - sqrt(5)) / 2.
TEST(OffsetCarriers, TwoShiftedLines) {
    Tower tower;
    const auto slanted = offset(tower, Segment{{0.0, 0.0}, {1.0, 2.0}}, 1.0);
    const auto level = offset(tower, Segment{{0.0, 0.0}, {1.0, 0.0}}, 1.0);
    // |(1, 0)| = 1: no new root for the level line.
    EXPECT_EQ(tower.size(), 1U);
    const auto meeting = meetChecked(tower, slanted, level);
    ASSERT_EQ(meeting.count, 1U);
    EXPECT_EQ(meeting.points[0].contact, Contact::crossing);
    const auto s5 = rootOf(tower, 5.0);
    EXPECT_TRUE(detail::samePoint(tower, meeting.points[0].point,
                                  point(number(1.0) - s5, number(2.0), 2.0)));
    expectRounded(tower, meeting.points[0].point, {-0.6180339887498949, 1.0});
}

TEST(OffsetCarriers, ParallelLinesAndTheSameLine) {
    Tower tower;
    const Segment segment{{0.0, 0.0}, {1.0, 2.0}};
    const auto line = offset(tower, segment, 1.0);
    // Parallel and distinct.
    const auto apart = meetChecked(tower, line, offset(tower, segment, 2.0));
    EXPECT_FALSE(apart.sameCarrier);
    EXPECT_EQ(apart.count, 0U);
    // The same line, with a direction twice as long: its constant carries
    // sqrt(20), a root of its own, and 10 sqrt(5) - 5 sqrt(20) is still zero.
    const auto longer = offset(tower, Segment{{0.0, 0.0}, {2.0, 4.0}}, 1.0);
    EXPECT_EQ(tower.size(), 2U);
    EXPECT_TRUE(meetChecked(tower, line, longer).sameCarrier);
    // The same line run backwards and moved to its right.
    const auto backwards = offset(tower, Segment{{3.0, 6.0}, {1.0, 2.0}}, -1.0);
    EXPECT_TRUE(meetChecked(tower, line, backwards).sameCarrier);
    // And a hair off it.
    EXPECT_FALSE(meetChecked(tower, line, offset(tower, segment, above(1.0))).sameCarrier);
}

// The circle of radius 3 moved inward by 1 and a clockwise arc of the same
// circle moved by -1 (to its right, which is inside) are the same circle of
// radius 2; moved by -1/2 they are concentric and apart.
TEST(OffsetCarriers, ConcentricCircles) {
    Tower tower;
    const auto circle = offset(tower, Circle{{0.0, 0.0}, 3.0}, 1.0);
    const Arc clockwise{{0.0, 0.0}, 3.0, 0.0, -1.0};
    const auto same = meetChecked(tower, circle, offset(tower, clockwise, -1.0));
    EXPECT_TRUE(same.sameCarrier);
    EXPECT_EQ(same.count, 0U);
    const auto apart = meetChecked(tower, circle, offset(tower, clockwise, -0.5));
    EXPECT_FALSE(apart.sameCarrier);
    EXPECT_EQ(apart.count, 0U);
    // Piece A about its own centre, against a circle there of a dyadic radius.
    const auto pieceA = offset(tower, kPieceA, 0.0);
    const auto around = meetChecked(tower, pieceA, offset(tower, Circle{{0.125, 0.875}, 1.0}, 0.0));
    EXPECT_FALSE(around.sameCarrier);
    EXPECT_EQ(around.count, 0U);
}

TEST(OffsetCarriers, NoMeeting) {
    Tower tower;
    const auto line = offset(tower, Segment{{0.0, 0.0}, {1.0, 2.0}}, 1.0);
    EXPECT_EQ(meetChecked(tower, line, offset(tower, Circle{{10.0, 0.0}, 3.0}, -1.0)).count, 0U);
    EXPECT_EQ(meetChecked(tower, offset(tower, kPieceA, 0.125),
                          offset(tower, Circle{{5.0, 5.0}, 1.0}, 0.25))
                  .count,
              0U);
}

// A distance of zero gives the carriers of carriers.hpp and their meetings:
// the same points (one root at most, converted), in the same order, with the
// same contacts.
TEST(OffsetCarriers, ZeroShiftGivesTheMeetingsOfCarriers) {
    const std::vector<Curve> curves{
        Segment{{0.0, 0.0}, {4.0, 0.0}},
        Segment{{8.0, 0.0}, {-2.0, 0.0}},
        Segment{{0.0, -1.0}, {3.0, 5.0}},
        Segment{{0.0, 2.0}, {4.0, 2.0}},
        Circle{{1.0, 1.0}, 2.0},
        Circle{{1.0, 0.0}, 2.0},
        Arc{{0.0, 0.0}, 3.0, 0.0, 2.0},
        Arc{{1.0, 1.0}, 2.0, 1.0, -2.0},
        kPieceA,
        kPieceB,
        PolylineSegment{{-1.0, 2.0}, {3.0, -2.0}, -0.3},
    };
    for (std::size_t i = 0; i < curves.size(); ++i) {
        for (std::size_t j = 0; j < curves.size(); ++j) {
            SCOPED_TRACE(testing::Message() << i << " " << j);
            Tower tower;
            const auto first = meaningOf(curves[i]);
            const auto second = meaningOf(curves[j]);
            const auto expected = detail::meet(detail::carrierOf(first), detail::carrierOf(second));
            const auto meeting =
                meetChecked(tower, detail::offsetCarrier(tower, first, 0.0).carrier,
                            detail::offsetCarrier(tower, second, 0.0).carrier);
            EXPECT_EQ(meeting.sameCarrier, expected.sameCarrier);
            ASSERT_EQ(meeting.count, expected.count);
            const auto found = meeting.found();
            const auto foundExpected = expected.found();
            for (std::size_t k = 0; k < found.size(); ++k) {
                const auto exact = converted(tower, foundExpected[k].point);
                EXPECT_TRUE(detail::samePoint(tower, found[k].point, exact)) << k;
                EXPECT_EQ(found[k].contact, foundExpected[k].contact) << k;
                const auto rounded = detail::roundedPoint(tower, found[k].point);
                const auto roundedExpected = detail::roundedPoint(foundExpected[k].point);
                expectWithinUlps(rounded.x, roundedExpected.x, 2);
                expectWithinUlps(rounded.y, roundedExpected.y, 2);
            }
        }
    }
}

// Deterministic pairs of slanted segments, circles and pieces moved by small
// distances: whatever the meeting, its points lie exactly on both carriers and
// the swap gives the same bits (meetChecked); the rounded points lie at the
// distance of the offset from the original line, or at the new radius.
TEST(OffsetCarriers, GeneratedPairs) {
    std::uint64_t state = 12;
    const auto next = [&state] {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<double>(static_cast<std::int64_t>((state >> 33U) % 17U) - 8);
    };
    const auto makeCurve = [&](std::uint64_t kind) -> Curve {
        switch (kind % 3) {
            case 0: {
                const Point2 start{next(), next()};
                Point2 end{next(), next()};
                if (end.x == start.x && end.y == start.y) {
                    end.x += 1.0;
                }
                return Segment{start, end};
            }
            case 1:
                return Circle{{next(), next()}, std::abs(next()) + 1.0};
            default: {
                const Point2 start{next(), next()};
                Point2 end{next(), next()};
                if (end.x == start.x && end.y == start.y) {
                    end.y += 1.0;
                }
                return PolylineSegment{start, end, (next() + 0.5) / 8.0};
            }
        }
    };
    std::size_t points = 0;
    for (std::uint64_t round = 0; round < 150; ++round) {
        SCOPED_TRACE(round);
        Tower tower;
        const Curve first = makeCurve(round);
        const Curve second = makeCurve(round / 3);
        const double d1 = next() / 4.0;
        const double d2 = next() / 4.0;
        const auto o1 = detail::offsetCarrier(tower, meaningOf(first), d1);
        const auto o2 = detail::offsetCarrier(tower, meaningOf(second), d2);
        if (o1.status != OffsetStatus::done || o2.status != OffsetStatus::done) {
            EXPECT_NE(o1.status, OffsetStatus::towerFull);
            EXPECT_NE(o2.status, OffsetStatus::towerFull);
            continue;
        }
        const auto meeting = meetChecked(tower, o1.carrier, o2.carrier);
        for (const auto& found : meeting.found()) {
            ++points;
            const auto p = detail::roundedPoint(tower, found.point);
            for (const auto& [curve, distance] : {std::pair{first, d1}, std::pair{second, d2}}) {
                if (const auto* segment = std::get_if<Segment>(&curve)) {
                    const double dx = segment->end.x - segment->start.x;
                    const double dy = segment->end.y - segment->start.y;
                    const double left =
                        (dx * (p.y - segment->start.y) - dy * (p.x - segment->start.x)) /
                        std::hypot(dx, dy);
                    EXPECT_NEAR(left, distance, 1e-9 * (1.0 + std::abs(p.x) + std::abs(p.y)));
                }
                if (const auto* circle = std::get_if<Circle>(&curve)) {
                    EXPECT_NEAR(std::hypot(p.x - circle->center.x, p.y - circle->center.y),
                                circle->radius - distance,
                                1e-9 * (1.0 + std::abs(p.x) + std::abs(p.y)));
                }
            }
        }
    }
    EXPECT_GT(points, 50U);
}

TEST(OffsetCarriers, SideOfTowerPoints) {
    Tower tower;
    const Segment segment{{0.0, 0.0}, {1.0, 2.0}};
    const auto origin = detail::towerPoint(Point2{0.0, 0.0});
    // The origin is right of the line moved to the left, left of the one moved
    // to the right, and on the line itself.
    EXPECT_EQ(detail::sideSign(tower, offset(tower, segment, 1.0), origin), Sign::negative);
    EXPECT_EQ(detail::sideSign(tower, offset(tower, segment, -1.0), origin), Sign::positive);
    EXPECT_EQ(detail::sideSign(tower, offset(tower, segment, 0.0), origin), Sign::zero);
    // A counterclockwise circle of radius 3 moved by 1: radius 2, inside is
    // left. A clockwise arc of it moved by 1: radius 4, inside is right.
    const auto circle = offset(tower, Circle{{0.0, 0.0}, 3.0}, 1.0);
    EXPECT_EQ(detail::sideSign(tower, circle, detail::towerPoint(Point2{1.5, 0.0})),
              Sign::positive);
    EXPECT_EQ(detail::sideSign(tower, circle, detail::towerPoint(Point2{0.0, -2.0})), Sign::zero);
    EXPECT_EQ(detail::sideSign(tower, circle, detail::towerPoint(Point2{3.0, 0.0})),
              Sign::negative);
    const auto arc = offset(tower, Arc{{0.0, 0.0}, 3.0, 0.0, -1.0}, 1.0);
    EXPECT_EQ(detail::sideSign(tower, arc, detail::towerPoint(Point2{3.5, 0.0})), Sign::negative);
    EXPECT_EQ(detail::sideSign(tower, arc, detail::towerPoint(Point2{0.0, 4.0})), Sign::zero);
    EXPECT_EQ(detail::sideSign(tower, arc, detail::towerPoint(Point2{5.0, 0.0})), Sign::positive);
    // With a distance of zero, the side of carriers.hpp, for points with one
    // root: the crossings of the circle of radius 2 with y = 1, (-+sqrt(3), 1).
    const auto meaning = meaningOf(Circle{{0.0, 0.0}, 2.0});
    const auto unshifted = detail::offsetCarrier(tower, meaning, 0.0).carrier;
    const auto crossings = detail::meet(
        detail::carrierOf(meaning), detail::carrierOf(meaningOf(Segment{{0.0, 1.0}, {1.0, 1.0}})));
    ASSERT_EQ(crossings.count, 2U);
    for (const auto& crossing : crossings.found()) {
        const auto exact = converted(tower, crossing.point);
        EXPECT_EQ(detail::sideSign(tower, unshifted, exact),
                  detail::sideSign(meaning, crossing.point));
        // At distance 2 from the centre: on the circle moved to radius 2.
        EXPECT_EQ(detail::sideSign(tower, circle, exact), Sign::zero);
    }
}

// The order along a line is the order of the projections; along a circle, the
// angle from the given point in the direction of the circle. The two points of
// SlantedSegmentAgainstShiftedCircle lie at about 213.4 and 93.4 degrees on the
// circle of radius 2; (sqrt(3), 1) at 30 degrees; (0, 2) at 90.
TEST(OffsetCarriers, OrderAlongCarriers) {
    Tower tower;
    const auto line = offset(tower, Segment{{0.0, 0.0}, {1.0, 2.0}}, 1.0);
    const auto circle = offset(tower, Circle{{0.0, 0.0}, 3.0}, 1.0);
    const auto meeting = meetChecked(tower, line, circle);
    ASSERT_EQ(meeting.count, 2U);
    const auto& lower = meeting.points[0].point;  // about 213.4 degrees
    const auto& upper = meeting.points[1].point;  // about 93.4 degrees
    const auto start = detail::towerPoint(Point2{2.0, 0.0});
    const auto top = detail::towerPoint(Point2{0.0, 2.0});
    const auto s3 = rootOf(tower, 3.0);
    const auto thirty = point(s3, number(1.0), 1.0);

    EXPECT_EQ(detail::compareAlong(tower, line, start, lower, upper), Sign::negative);
    EXPECT_EQ(detail::compareAlong(tower, line, upper, upper, lower), Sign::positive);
    EXPECT_EQ(detail::compareAlong(tower, line, start, lower, lower), Sign::zero);

    EXPECT_EQ(detail::compareAlong(tower, circle, start, upper, lower), Sign::negative);
    EXPECT_EQ(detail::compareAlong(tower, circle, start, thirty, top), Sign::negative);
    EXPECT_EQ(detail::compareAlong(tower, circle, start, top, upper), Sign::negative);
    EXPECT_EQ(detail::compareAlong(tower, circle, start, start, thirty), Sign::negative);
    // From the upper point, the lower one comes before the start, and the
    // upper point itself first of all.
    EXPECT_EQ(detail::compareAlong(tower, circle, upper, lower, start), Sign::negative);
    EXPECT_EQ(detail::compareAlong(tower, circle, upper, upper, lower), Sign::negative);
    EXPECT_EQ(detail::compareAlong(tower, circle, upper, top, start), Sign::positive);
    // The same direction at another radius is the same position.
    const auto twiceThirty = point(s3.timesPowerOfTwo(1), number(2.0), 1.0);
    EXPECT_EQ(detail::compareAlong(tower, circle, start, thirty, twiceThirty), Sign::zero);
    // A clockwise arc of the same circle runs the other way.
    const auto clockwise = offset(tower, Arc{{0.0, 0.0}, 3.0, 0.0, -1.0}, -1.0);
    EXPECT_EQ(detail::compareAlong(tower, clockwise, start, upper, lower), Sign::positive);
    EXPECT_EQ(detail::compareAlong(tower, clockwise, start, thirty, top), Sign::positive);
    EXPECT_EQ(detail::compareAlong(tower, clockwise, start, start, top), Sign::negative);
    // A point of carriers.hpp, with one root: (sqrt(3), 1) as a meeting of P9.
    const auto meaning = meaningOf(Circle{{0.0, 0.0}, 2.0});
    const auto crossings = detail::meet(
        detail::carrierOf(meaning), detail::carrierOf(meaningOf(Segment{{0.0, 1.0}, {1.0, 1.0}})));
    ASSERT_EQ(crossings.count, 2U);
    const auto exact = converted(tower, crossings.found()[1].point);
    EXPECT_EQ(detail::compareAlong(tower, circle, start, exact, thirty), Sign::zero);
    EXPECT_EQ(detail::compareAlong(tower, circle, start, exact, upper), Sign::negative);
}

TEST(OffsetCarriers, DistancesFromAPoint) {
    Tower tower;
    const auto origin = detail::towerPoint(Point2{0.0, 0.0});
    const auto s6 = rootOf(tower, 6.0);
    const auto s21 = rootOf(tower, 21.0);
    const auto threeFour = detail::towerPoint(Point2{3.0, 4.0});
    // |(2 sqrt(6), 1)| = |(sqrt(21), 2)| = |(3, 4)| = 5.
    EXPECT_EQ(detail::compareDistances(tower, origin, threeFour,
                                       point(s6.timesPowerOfTwo(1), number(1.0), 1.0)),
              Sign::zero);
    EXPECT_EQ(detail::compareDistances(tower, origin, point(s21, number(2.0), 1.0), threeFour),
              Sign::zero);
    EXPECT_EQ(detail::compareDistances(tower, origin, point(s21, number(2.0), 1.0),
                                       detail::towerPoint(Point2{3.0, above(4.0)})),
              Sign::negative);
    // The meetings of the shifted line and circle lie at 2 from the centre;
    // seen from the lower one, the upper one is farther than the centre.
    const auto line = offset(tower, Segment{{0.0, 0.0}, {1.0, 2.0}}, 1.0);
    const auto circle = offset(tower, Circle{{0.0, 0.0}, 3.0}, 1.0);
    const auto meeting = meetChecked(tower, line, circle);
    ASSERT_EQ(meeting.count, 2U);
    EXPECT_EQ(detail::compareDistances(tower, origin, meeting.points[0].point,
                                       detail::towerPoint(Point2{2.0, 0.0})),
              Sign::zero);
    EXPECT_EQ(
        detail::compareDistances(tower, meeting.points[0].point, meeting.points[1].point, origin),
        Sign::positive);
}

// y = 2 against y = 2^-60 x moved up by 1: x = (2 - sqrt(1 + 2^-120)) 2^60,
// just below 2^60, beyond the domain.
TEST(OffsetCarriers, BeyondTheDomainAndRounding) {
    Tower tower;
    const auto level = offset(tower, Segment{{0.0, 0.0}, {1.0, 0.0}}, 2.0);
    const auto shallow = offset(tower, Segment{{0.0, 0.0}, {1.0, 0x1p-60}}, 1.0);
    const auto meeting = meetChecked(tower, level, shallow);
    ASSERT_EQ(meeting.count, 1U);
    EXPECT_TRUE(detail::beyondDomain(tower, meeting.points[0].point));
    const auto rounded = detail::roundedPoint(tower, meeting.points[0].point);
    EXPECT_EQ(rounded.x, 0x1p60);
    EXPECT_EQ(rounded.y, 2.0);
    EXPECT_FALSE(detail::beyondDomain(tower, detail::towerPoint(Point2{0x1p50, -0x1p50})));
    EXPECT_TRUE(detail::beyondDomain(tower, detail::towerPoint(Point2{above(0x1p50), 0.0})));
    EXPECT_TRUE(detail::beyondDomain(tower, detail::towerPoint(Point2{0.0, below(-0x1p50)})));
}

// The new radius is compared with zero exactly, also when it is irrational.
TEST(OffsetCarriers, VanishingRadius) {
    Tower tower;
    const auto status = [&tower](const Curve& curve, double distance) {
        return detail::offsetCarrier(tower, meaningOf(curve), distance).status;
    };
    const Arc counterclockwise{{1.0, 1.0}, 2.0, 0.5, 1.0};
    const Arc clockwise{{1.0, 1.0}, 2.0, 0.5, -1.0};
    EXPECT_EQ(status(counterclockwise, 2.0), OffsetStatus::vanished);
    EXPECT_EQ(status(counterclockwise, below(2.0)), OffsetStatus::done);
    EXPECT_EQ(status(counterclockwise, -100.0), OffsetStatus::done);
    EXPECT_EQ(status(clockwise, -2.0), OffsetStatus::vanished);
    EXPECT_EQ(status(clockwise, above(-2.0)), OffsetStatus::done);
    EXPECT_EQ(status(Circle{{0.0, 0.0}, 2.0}, 3.0), OffsetStatus::vanished);
    // R = 5 sqrt(2) / 8 lies just below the nearest double.
    const double nearest = kRadiusOfA;
    EXPECT_EQ(status(kPieceA, nearest), OffsetStatus::vanished);
    EXPECT_EQ(status(kPieceA, below(nearest)), OffsetStatus::done);
    EXPECT_EQ(status(kPieceB, -nearest), OffsetStatus::vanished);
    EXPECT_EQ(status(kPieceB, -below(nearest)), OffsetStatus::done);
    const auto meaningA = meaningOf(kPieceA);
    const auto& arc = std::get<ArcMeaning>(meaningA.support);
    EXPECT_EQ(detail::offsetRadiusSign(arc, nearest), Sign::negative);
    EXPECT_EQ(detail::offsetRadiusSign(arc, below(nearest)), Sign::positive);
    const auto meaningArc = meaningOf(counterclockwise);
    EXPECT_EQ(detail::offsetRadiusSign(std::get<ArcMeaning>(meaningArc.support), 2.0), Sign::zero);
    // Only the carriers that are done adjoin a root.
    EXPECT_EQ(tower.size(), 1U);
}

// When the tower is full, a new root is refused and the functions say so;
// carriers and meetings that need no new root still work.
TEST(OffsetCarriers, FullTower) {
    Tower tower;
    for (const double radicand : {2.0, 3.0, 5.0, 7.0, 11.0, 13.0}) {
        static_cast<void>(rootOf(tower, radicand));
    }
    ASSERT_EQ(tower.size(), kMaxTowerRoots);
    const auto meaning = meaningOf(Segment{{0.0, 0.0}, {1.0, 3.0}});
    EXPECT_EQ(detail::offsetCarrier(tower, meaning, 1.0).status, OffsetStatus::towerFull);
    EXPECT_EQ(detail::offsetCarrier(tower, meaning, 0.0).status, OffsetStatus::done);
    EXPECT_EQ(detail::offsetCarrier(tower, meaningOf(kPieceA), 0.125).status,
              OffsetStatus::towerFull);
    // sqrt(5) is there already, and |(3, 4)| = 5 needs none.
    const auto slanted = offset(tower, Segment{{0.0, 0.0}, {1.0, 2.0}}, 1.0);
    const auto level = offset(tower, Segment{{0.0, 0.0}, {3.0, 4.0}}, 1.0);
    EXPECT_EQ(meetChecked(tower, slanted, level).count, 1U);
    // Two crossings with an irrational discriminant need a root that does not
    // fit.
    const auto circle = offset(tower, Circle{{0.0, 0.0}, 3.0}, 0.0);
    EXPECT_FALSE(
        detail::meetOffset(tower, offset(tower, Segment{{0.0, 0.0}, {1.0, 0.0}}, 1.0), circle)
            .has_value());
    // A tangency needs no root.
    const auto tangent =
        detail::meetOffset(tower, offset(tower, Segment{{0.0, 0.0}, {1.0, 0.0}}, 3.0), circle);
    EXPECT_EQ(tangent.value_or(OffsetMeeting{}).count, 1U);
    // A point of carriers.hpp with a new root does not fit either.
    const auto crossings =
        detail::meet(detail::carrierOf(meaningOf(Circle{{0.0, 0.0}, 3.0})),
                     detail::carrierOf(meaningOf(Segment{{0.0, 1.0}, {1.0, 1.0}})));
    ASSERT_EQ(crossings.count, 2U);
    EXPECT_FALSE(detail::towerPoint(tower, crossings.points[0].point).has_value());
}

}  // namespace
