// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Unit tests of the exact meetings of carriers (stage 0.2, package P9). The
// expected points are computed by hand in the comments; every point found is
// also checked against both carriers with exact arithmetic written here, apart
// from carriers.cpp: by substitution in the equation of a line or of a circle
// given by centre and radius, and by the in-circle determinant of the three
// points start, m and end for a curved polyline segment.

#include "carriers.hpp"

#include "curve_meaning.hpp"
#include "exact/quadratic.hpp"

#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
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
using solid_space::geom2d::detail::Carrier;
using solid_space::geom2d::detail::CarrierMeeting;
using solid_space::geom2d::detail::Dyadic;
using solid_space::geom2d::detail::ExactPoint;
using solid_space::robust::Sign;

namespace detail = solid_space::geom2d::detail;
namespace exact = solid_space::geom2d::exact;

constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kMaxCoordinate = 0x1p50;

double below(double value) {
    return std::nextafter(value, -kInfinity);
}

double above(double value) {
    return std::nextafter(value, kInfinity);
}

Carrier carrierOf(const Curve& curve) {
    const auto meaning = detail::meaningOf(curve);
    EXPECT_FALSE(meaning.degenerate);
    return detail::carrierOf(meaning);
}

CarrierMeeting meet(const Curve& first, const Curve& second) {
    return detail::meet(carrierOf(first), carrierOf(second));
}

// --- Independent exact checks -------------------------------------------

// A number a + b sqrt(d) of the field of one exact point.
struct Surd {
    Dyadic a;
    Dyadic b;
};

Surd add(const Surd& x, const Surd& y) {
    return {x.a + y.a, x.b + y.b};
}

Surd subtract(const Surd& x, const Surd& y) {
    return {x.a - y.a, x.b - y.b};
}

Surd multiply(const Surd& x, const Surd& y, const Dyadic& d) {
    return {x.a * y.a + x.b * y.b * d, x.a * y.b + x.b * y.a};
}

// The point scaled by its weight: (r.x + q.x sqrt(d), r.y + q.y sqrt(d)).
Surd scaledX(const ExactPoint& p) {
    return {p.rational.x, p.radical.x};
}

Surd scaledY(const ExactPoint& p) {
    return {p.rational.y, p.radical.y};
}

// A double times the weight of the point.
Surd weighted(double value, const ExactPoint& p) {
    return {Dyadic(value) * p.weight, Dyadic()};
}

Sign signOf(const Surd& x, const ExactPoint& p) {
    return exact::signOf(x.a, x.b, p.radicand);
}

// (b - a) x (p - a), times the weight of p.
Sign lineValue(Point2 a, Point2 b, const ExactPoint& p) {
    const Surd ux{Dyadic(b.x) - Dyadic(a.x), Dyadic()};
    const Surd uy{Dyadic(b.y) - Dyadic(a.y), Dyadic()};
    const auto vx = subtract(scaledX(p), weighted(a.x, p));
    const auto vy = subtract(scaledY(p), weighted(a.y, p));
    return signOf(subtract(multiply(ux, vy, p.radicand), multiply(uy, vx, p.radicand)), p);
}

// |p - c|^2 - r^2, times the squared weight of p.
Sign circleValue(Point2 center, double radius, const ExactPoint& p) {
    const auto dx = subtract(scaledX(p), weighted(center.x, p));
    const auto dy = subtract(scaledY(p), weighted(center.y, p));
    const auto r = weighted(radius, p);
    const auto value = subtract(add(multiply(dx, dx, p.radicand), multiply(dy, dy, p.radicand)),
                                multiply(r, r, p.radicand));
    return signOf(value, p);
}

// The in-circle determinant of a, b, c and p, all scaled by the weight of p:
// zero exactly when p lies on the circle through a, b and c.
Sign inCircleValue(const std::array<detail::ExactVector, 3>& points, const ExactPoint& p) {
    const auto& d = p.radicand;
    std::array<std::array<Surd, 3>, 3> rows;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const auto x = subtract(Surd{points[i].x * p.weight, Dyadic()}, scaledX(p));
        const auto y = subtract(Surd{points[i].y * p.weight, Dyadic()}, scaledY(p));
        rows[i] = {x, y, add(multiply(x, x, d), multiply(y, y, d))};
    }
    const auto minor = [&](std::size_t i, std::size_t j) {
        return subtract(multiply(rows[1][i], rows[2][j], d), multiply(rows[1][j], rows[2][i], d));
    };
    const auto value =
        add(subtract(multiply(rows[0][0], minor(1, 2), d), multiply(rows[0][1], minor(0, 2), d)),
            multiply(rows[0][2], minor(0, 1), d));
    return signOf(value, p);
}

// The exact sign of a point with respect to the carrier of a curve, computed
// here and not with carriers.cpp.
Sign independentSign(const Curve& curve, const ExactPoint& p) {
    if (const auto* segment = std::get_if<Segment>(&curve)) {
        return lineValue(segment->start, segment->end, p);
    }
    if (const auto* circle = std::get_if<Circle>(&curve)) {
        return circleValue(circle->center, circle->radius, p);
    }
    if (const auto* arc = std::get_if<Arc>(&curve)) {
        return circleValue(arc->center, arc->radius, p);
    }
    const auto& piece = std::get<PolylineSegment>(curve);
    if (piece.bulge == 0.0) {
        return lineValue(piece.start, piece.end, p);
    }
    const detail::ExactVector start{Dyadic(piece.start.x), Dyadic(piece.start.y)};
    const detail::ExactVector end{Dyadic(piece.end.x), Dyadic(piece.end.y)};
    const auto dx = end.x - start.x;
    const auto dy = end.y - start.y;
    const auto halfBulge = Dyadic(piece.bulge).timesPowerOfTwo(-1);
    const detail::ExactVector m{(start.x + end.x).timesPowerOfTwo(-1) + halfBulge * dy,
                                (start.y + end.y).timesPowerOfTwo(-1) - halfBulge * dx};
    return inCircleValue({start, m, end}, p);
}

void expectOnBoth(const Curve& first, const Curve& second, const CarrierMeeting& meeting) {
    for (const auto& found : meeting.found()) {
        EXPECT_EQ(independentSign(first, found.point), Sign::zero);
        EXPECT_EQ(independentSign(second, found.point), Sign::zero);
        EXPECT_TRUE(detail::onCarrier(carrierOf(first), found.point));
        EXPECT_TRUE(detail::onCarrier(carrierOf(second), found.point));
    }
}

bool sameDyadic(const Dyadic& a, const Dyadic& b) {
    return exact::compare(a, b) == Sign::zero;
}

bool sameBits(double a, double b) {
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

// meet(b, a) gives the same points in the same order, with the same
// representation and the same rounded bits.
void expectSymmetric(const Curve& first, const Curve& second) {
    const auto ab = meet(first, second);
    const auto ba = meet(second, first);
    ASSERT_EQ(ab.sameCarrier, ba.sameCarrier);
    ASSERT_EQ(ab.count, ba.count);
    for (std::size_t i = 0; i < ab.count; ++i) {
        const auto& p = ab.points[i];
        const auto& q = ba.points[i];
        EXPECT_EQ(p.contact, q.contact);
        EXPECT_TRUE(sameDyadic(p.point.rational.x, q.point.rational.x));
        EXPECT_TRUE(sameDyadic(p.point.rational.y, q.point.rational.y));
        EXPECT_TRUE(sameDyadic(p.point.radical.x, q.point.radical.x));
        EXPECT_TRUE(sameDyadic(p.point.radical.y, q.point.radical.y));
        EXPECT_TRUE(sameDyadic(p.point.radicand, q.point.radicand));
        EXPECT_TRUE(sameDyadic(p.point.weight, q.point.weight));
        const auto rp = detail::roundedPoint(p.point);
        const auto rq = detail::roundedPoint(q.point);
        EXPECT_TRUE(sameBits(rp.x, rq.x));
        EXPECT_TRUE(sameBits(rp.y, rq.y));
    }
}

void expectPoint(const ExactPoint& found, Point2 expected) {
    EXPECT_TRUE(detail::samePoint(found, detail::exactPoint(expected)));
    const auto rounded = detail::roundedPoint(found);
    EXPECT_TRUE(sameBits(rounded.x, expected.x)) << rounded.x << " vs " << expected.x;
    EXPECT_TRUE(sameBits(rounded.y, expected.y)) << rounded.y << " vs " << expected.y;
}

// --- Two lines -------------------------------------------------------------

TEST(Carriers, LinesCrossAtARationalPoint) {
    // Request #20, case 1: (0,0)-(10,10) and (0,10)-(10,0) cross at (5, 5).
    const Curve first = Segment{{0.0, 0.0}, {10.0, 10.0}};
    const Curve second = Segment{{0.0, 10.0}, {10.0, 0.0}};
    const auto meeting = meet(first, second);
    EXPECT_FALSE(meeting.sameCarrier);
    ASSERT_EQ(meeting.count, 1U);
    EXPECT_EQ(meeting.points[0].contact, Contact::crossing);
    expectPoint(meeting.points[0].point, {5.0, 5.0});
    EXPECT_EQ(meeting.points[0].point.weight.sign(), Sign::positive);
    expectOnBoth(first, second, meeting);
    expectSymmetric(first, second);
}

TEST(Carriers, LinesCrossAtAPointThatIsNotADouble) {
    // y = x and y = 1 - 2x meet at (1/3, 1/3): rounded to the nearest double.
    const Curve first = Segment{{0.0, 0.0}, {1.0, 1.0}};
    const Curve second = PolylineSegment{{0.0, 1.0}, {1.0, -1.0}, 0.0};
    const auto meeting = meet(first, second);
    ASSERT_EQ(meeting.count, 1U);
    const auto rounded = detail::roundedPoint(meeting.points[0].point);
    EXPECT_EQ(rounded.x, 1.0 / 3.0);
    EXPECT_EQ(rounded.y, 1.0 / 3.0);
    EXPECT_FALSE(detail::samePoint(meeting.points[0].point, detail::exactPoint(rounded)));
    expectOnBoth(first, second, meeting);
    expectSymmetric(first, second);
}

TEST(Carriers, ParallelLinesDoNotMeet) {
    const Curve oblique = Segment{{0.0, 0.0}, {3.0, 1.0}};
    const Curve parallel = Segment{{1.0, 1.0}, {7.0, 3.0}};
    const auto meeting = meet(oblique, parallel);
    EXPECT_FALSE(meeting.sameCarrier);
    EXPECT_EQ(meeting.count, 0U);
    expectSymmetric(oblique, parallel);
    // The x axis and the line one subnormal above it.
    const Curve axis = Segment{{0.0, 0.0}, {3.0, 0.0}};
    const Curve lifted = Segment{{1.0, 0x1p-1074}, {7.0, 0x1p-1074}};
    EXPECT_FALSE(meet(axis, lifted).sameCarrier);
    EXPECT_EQ(meet(axis, lifted).count, 0U);
    // Tilted by one subnormal: they meet, far away.
    const Curve tilted = Segment{{0.0, 0x1p-1074}, {3.0, 0x1p-1073}};
    const auto crossing = meet(axis, tilted);
    ASSERT_EQ(crossing.count, 1U);
    expectPoint(crossing.points[0].point, {-3.0, 0.0});
    expectOnBoth(axis, tilted, crossing);
}

TEST(Carriers, SameLineInBothDirections) {
    const Curve first = Segment{{0.0, 0.0}, {3.0, 1.0}};
    const Curve backwards = PolylineSegment{{9.0, 3.0}, {-6.0, -2.0}, 0.0};
    const auto meeting = meet(first, backwards);
    EXPECT_TRUE(meeting.sameCarrier);
    EXPECT_EQ(meeting.count, 0U);
    expectSymmetric(first, backwards);
    // One ulp off the line at one end: the two lines cross at (-6, -2).
    const Curve tilted = Segment{{9.0, above(3.0)}, {-6.0, -2.0}};
    const auto crossing = meet(first, tilted);
    EXPECT_FALSE(crossing.sameCarrier);
    ASSERT_EQ(crossing.count, 1U);
    expectPoint(crossing.points[0].point, {-6.0, -2.0});
    expectOnBoth(first, tilted, crossing);
}

TEST(Carriers, AlmostParallelLinesMeetFarAway) {
    // y = 0 and the line through (0, 1) and (1, 1 - 2^-49): they meet at
    // x = 2^49, inside the domain.
    const Curve axis = Segment{{0.0, 0.0}, {1.0, 0.0}};
    const Curve inside = Segment{{0.0, 1.0}, {1.0, 1.0 - 0x1p-49}};
    const auto near = meet(axis, inside);
    ASSERT_EQ(near.count, 1U);
    expectPoint(near.points[0].point, {0x1p49, 0.0});
    EXPECT_FALSE(detail::beyondDomain(near.points[0].point));
    expectOnBoth(axis, inside, near);
    // With 2^-52 they meet at x = 2^52, beyond kMaxCoordinate: still exact,
    // and on both lines.
    const Curve beyond = Segment{{0.0, 1.0}, {1.0, 1.0 - 0x1p-52}};
    const auto far = meet(axis, beyond);
    ASSERT_EQ(far.count, 1U);
    EXPECT_TRUE(detail::beyondDomain(far.points[0].point));
    expectPoint(far.points[0].point, {0x1p52, 0.0});
    expectOnBoth(axis, beyond, far);
    expectSymmetric(axis, beyond);
    // The line from (0, 0) to (2^50, 2^-1074) has slope 2^-1124: it meets
    // y = 2^40 at x = 2^1164, beyond every double.
    const Curve tiny = Segment{{0.0, 0.0}, {kMaxCoordinate, 0x1p-1074}};
    const Curve high = Segment{{0.0, 0x1p40}, {1.0, 0x1p40}};
    const auto beyondDoubles = meet(tiny, high);
    ASSERT_EQ(beyondDoubles.count, 1U);
    EXPECT_TRUE(detail::beyondDomain(beyondDoubles.points[0].point));
    const auto rounded = detail::roundedPoint(beyondDoubles.points[0].point);
    EXPECT_EQ(rounded.x, kInfinity);
    EXPECT_EQ(rounded.y, 0x1p40);
    expectOnBoth(tiny, high, beyondDoubles);
    expectSymmetric(tiny, high);
}

// --- A line and a circle ---------------------------------------------------

TEST(Carriers, LineCrossesCircleInTheDirectionOfTheLine) {
    // y = 4 and the circle of radius 5: (-3, 4) and (3, 4).
    const Curve circle = Circle{{0.0, 0.0}, 5.0};
    const Curve line = Segment{{-10.0, 4.0}, {10.0, 4.0}};
    const auto meeting = meet(line, circle);
    ASSERT_EQ(meeting.count, 2U);
    EXPECT_EQ(meeting.points[0].contact, Contact::crossing);
    EXPECT_EQ(meeting.points[1].contact, Contact::crossing);
    expectPoint(meeting.points[0].point, {-3.0, 4.0});
    expectPoint(meeting.points[1].point, {3.0, 4.0});
    expectOnBoth(line, circle, meeting);
    expectSymmetric(line, circle);
    // The other way along the line: (3, 4) first.
    const Curve back = Segment{{10.0, 4.0}, {-10.0, 4.0}};
    const auto reversed = meet(circle, back);
    ASSERT_EQ(reversed.count, 2U);
    expectPoint(reversed.points[0].point, {3.0, 4.0});
    expectPoint(reversed.points[1].point, {-3.0, 4.0});
}

TEST(Carriers, LineTangentToCircleAndOneUlpAway) {
    // Request #20, case 2: the circle of radius 5 against y = 5 and y = 5 +- 1 ulp.
    const Curve circle = Circle{{0.0, 0.0}, 5.0};
    const Curve tangent = Segment{{-1.0, 5.0}, {1.0, 5.0}};
    const auto touching = meet(circle, tangent);
    ASSERT_EQ(touching.count, 1U);
    EXPECT_EQ(touching.points[0].contact, Contact::touching);
    expectPoint(touching.points[0].point, {0.0, 5.0});
    expectOnBoth(circle, tangent, touching);
    expectSymmetric(circle, tangent);

    const Curve outside = Segment{{-1.0, above(5.0)}, {1.0, above(5.0)}};
    const auto none = meet(circle, outside);
    EXPECT_FALSE(none.sameCarrier);
    EXPECT_EQ(none.count, 0U);

    // y = 5 - 2^-50: x = +-sqrt(25 - y^2) = +-sqrt(10 * 2^-50 - 2^-100).
    const double y = below(5.0);
    ASSERT_EQ(y, 5.0 - 0x1p-50);
    const Curve inside = Segment{{-1.0, y}, {1.0, y}};
    const auto two = meet(circle, inside);
    ASSERT_EQ(two.count, 2U);
    expectOnBoth(circle, inside, two);
    expectSymmetric(circle, inside);
    const double x = std::sqrt(10.0 * 0x1p-50 - 0x1p-100);
    const auto left = detail::roundedPoint(two.points[0].point);
    const auto right = detail::roundedPoint(two.points[1].point);
    EXPECT_EQ(left.y, y);
    EXPECT_EQ(right.y, y);
    EXPECT_NEAR(left.x, -x, 2.0 * 0x1p-52 * x);
    EXPECT_NEAR(right.x, x, 2.0 * 0x1p-52 * x);
    EXPECT_EQ(two.points[0].contact, Contact::crossing);
}

TEST(Carriers, LineAgainstTheCircleOfACurvedPiece) {
    // The piece (-5, 0) -> (5, 0) with bulge 1 lies on the circle of radius 5
    // about the origin, with weight 4 in homogeneous coordinates.
    const Curve piece = PolylineSegment{{-5.0, 0.0}, {5.0, 0.0}, 1.0};
    const Curve line = Segment{{0.0, 4.0}, {1.0, 4.0}};
    const auto meeting = meet(line, piece);
    ASSERT_EQ(meeting.count, 2U);
    expectPoint(meeting.points[0].point, {-3.0, 4.0});
    expectPoint(meeting.points[1].point, {3.0, 4.0});
    expectOnBoth(line, piece, meeting);
    expectSymmetric(line, piece);
    const Curve tangent = Segment{{0.0, -5.0}, {1.0, -5.0}};
    const auto touching = meet(piece, tangent);
    ASSERT_EQ(touching.count, 1U);
    EXPECT_EQ(touching.points[0].contact, Contact::touching);
    expectPoint(touching.points[0].point, {0.0, -5.0});
}

// --- Two circles -----------------------------------------------------------

TEST(Carriers, CirclesCrossOnTheRightFirst) {
    // Radius 5 about (0, 0) and (6, 0): (3, -4) and (3, 4). The centre (0, 0)
    // comes first in the order of the data, and (3, -4) is on the right of the
    // line from it to (6, 0).
    const Curve first = Circle{{0.0, 0.0}, 5.0};
    const Curve second = Circle{{6.0, 0.0}, 5.0};
    for (const auto& meeting : {meet(first, second), meet(second, first)}) {
        ASSERT_EQ(meeting.count, 2U);
        expectPoint(meeting.points[0].point, {3.0, -4.0});
        expectPoint(meeting.points[1].point, {3.0, 4.0});
        EXPECT_EQ(meeting.points[0].contact, Contact::crossing);
    }
    expectOnBoth(first, second, meet(first, second));
    expectSymmetric(first, second);
}

TEST(Carriers, CirclesTangentOutsideAndOneUlpAway) {
    // Radii 5 and 3, centres 8 apart: tangent outside at (5, 0).
    const Curve big = Circle{{0.0, 0.0}, 5.0};
    const Curve small = Circle{{8.0, 0.0}, 3.0};
    const auto touching = meet(big, small);
    ASSERT_EQ(touching.count, 1U);
    EXPECT_EQ(touching.points[0].contact, Contact::touching);
    expectPoint(touching.points[0].point, {5.0, 0.0});
    expectOnBoth(big, small, touching);
    expectSymmetric(big, small);

    const Curve apart = Circle{{above(8.0), 0.0}, 3.0};
    EXPECT_EQ(meet(big, apart).count, 0U);
    const Curve closer = Circle{{below(8.0), 0.0}, 3.0};
    const auto two = meet(big, closer);
    ASSERT_EQ(two.count, 2U);
    EXPECT_EQ(two.points[0].contact, Contact::crossing);
    EXPECT_EQ(detail::compareY(two.points[0].point, two.points[1].point), Sign::negative);
    expectOnBoth(big, closer, two);
    expectSymmetric(big, closer);
}

TEST(Carriers, CirclesTangentInsideAndOneUlpAway) {
    // Radii 5 and 3, centres 2 apart: tangent inside at (5, 0).
    const Curve big = Circle{{0.0, 0.0}, 5.0};
    const Curve small = Arc{{2.0, 0.0}, 3.0, 0.0, 1.0};
    const auto touching = meet(small, big);
    ASSERT_EQ(touching.count, 1U);
    EXPECT_EQ(touching.points[0].contact, Contact::touching);
    expectPoint(touching.points[0].point, {5.0, 0.0});
    expectOnBoth(big, small, touching);
    expectSymmetric(big, small);

    // A larger small circle pokes out: two crossings; a smaller one stays
    // inside: none.
    const Curve larger = Circle{{2.0, 0.0}, above(3.0)};
    const auto two = meet(big, larger);
    ASSERT_EQ(two.count, 2U);
    expectOnBoth(big, larger, two);
    const Curve smaller = Circle{{2.0, 0.0}, below(3.0)};
    EXPECT_EQ(meet(big, smaller).count, 0U);
}

TEST(Carriers, ConcentricCircles) {
    const Curve first = Circle{{1.0, 2.0}, 3.0};
    const Curve same = Arc{{1.0, 2.0}, 3.0, 1.0, -2.0};
    const auto meeting = meet(first, same);
    EXPECT_TRUE(meeting.sameCarrier);
    EXPECT_EQ(meeting.count, 0U);
    const Curve wider = Circle{{1.0, 2.0}, above(3.0)};
    const auto none = meet(first, wider);
    EXPECT_FALSE(none.sameCarrier);
    EXPECT_EQ(none.count, 0U);
}

TEST(Carriers, TheCircleOfAPieceIsTheSameAsACircle) {
    // Both bulges of the half circles of radius 5 about the origin, and a
    // quarter circle with bulge tan(pi / 8) = sqrt(2) - 1 rounded: only the
    // first two are exactly that circle.
    const Curve circle = Circle{{0.0, 0.0}, 5.0};
    const Curve upper = PolylineSegment{{5.0, 0.0}, {-5.0, 0.0}, 1.0};
    const Curve lower = PolylineSegment{{-5.0, 0.0}, {5.0, 0.0}, 1.0};
    EXPECT_TRUE(meet(circle, upper).sameCarrier);
    EXPECT_TRUE(meet(lower, circle).sameCarrier);
    EXPECT_TRUE(meet(upper, lower).sameCarrier);
    const Curve quarter = PolylineSegment{{5.0, 0.0}, {0.0, 5.0}, std::sqrt(2.0) - 1.0};
    const auto meeting = meet(quarter, circle);
    EXPECT_FALSE(meeting.sameCarrier);
    // It still passes through its two ends, which lie on the circle.
    ASSERT_EQ(meeting.count, 2U);
    expectOnBoth(quarter, circle, meeting);
    expectSymmetric(quarter, circle);
}

TEST(Carriers, TwoCurvedPieces) {
    // Two pieces of radius-5 circles, centres (0, 0) and (6, 0), with
    // weights other than 1: (3, -4) and (3, 4).
    const Curve first = PolylineSegment{{0.0, 5.0}, {0.0, -5.0}, -1.0};
    const Curve second = PolylineSegment{{6.0, -5.0}, {6.0, 5.0}, -1.0};
    const auto meeting = meet(first, second);
    ASSERT_EQ(meeting.count, 2U);
    expectPoint(meeting.points[0].point, {3.0, -4.0});
    expectPoint(meeting.points[1].point, {3.0, 4.0});
    expectOnBoth(first, second, meeting);
    expectSymmetric(first, second);
}

// --- Exact points ----------------------------------------------------------

TEST(Carriers, PointsWithDifferentRadicalsCompare) {
    // (3, 4) as a meeting of a line and a circle and as a meeting of two
    // circles: two representations, with different radicands, of one point.
    const Curve circle = Circle{{0.0, 0.0}, 5.0};
    const auto fromLine = meet(Segment{{-1.0, 4.0}, {1.0, 4.0}}, circle).points[1].point;
    const auto fromCircles = meet(circle, Circle{{6.0, 0.0}, 5.0}).points[1].point;
    ASSERT_FALSE(sameDyadic(fromLine.radicand, fromCircles.radicand));
    EXPECT_TRUE(detail::samePoint(fromLine, fromCircles));
    EXPECT_TRUE(detail::samePoint(fromCircles, detail::exactPoint({3.0, 4.0})));
    EXPECT_FALSE(detail::samePoint(fromCircles, detail::exactPoint({3.0, above(4.0)})));
    EXPECT_EQ(detail::compareY(fromCircles, detail::exactPoint({3.0, above(4.0)})), Sign::negative);
    EXPECT_EQ(detail::compareX(fromLine, detail::exactPoint({below(3.0), 4.0})), Sign::positive);

    // Irrational points: y = 1 and the circle of radius 2 meet at
    // (+-sqrt(3), 1); x = 1 and the same circle at (1, +-sqrt(3)). Compare
    // sqrt(3) with itself through two radicands, and with doubles around it.
    const Curve two = Circle{{0.0, 0.0}, 2.0};
    const auto horizontal = meet(Segment{{0.0, 1.0}, {1.0, 1.0}}, two);
    const auto vertical = meet(Segment{{1.0, 0.0}, {1.0, 1.0}}, two);
    ASSERT_EQ(horizontal.count, 2U);
    ASSERT_EQ(vertical.count, 2U);
    const auto& a = horizontal.points[1].point;  // (sqrt(3), 1)
    const auto& b = vertical.points[1].point;    // (1, sqrt(3))
    EXPECT_EQ(detail::compareX(a, b), Sign::positive);
    EXPECT_EQ(detail::compareY(a, b), Sign::negative);
    EXPECT_FALSE(detail::samePoint(a, b));
    // std::sqrt is correctly rounded, so sqrt(3) lies strictly between the
    // doubles around it and is none of them.
    const double root = std::sqrt(3.0);
    EXPECT_NE(detail::compareX(a, detail::exactPoint({root, 1.0})), Sign::zero);
    EXPECT_EQ(detail::compareX(a, detail::exactPoint({below(root), 1.0})), Sign::positive);
    EXPECT_EQ(detail::compareX(a, detail::exactPoint({above(root), 1.0})), Sign::negative);
    EXPECT_EQ(detail::compareY(b, detail::exactPoint({0.0, below(root)})), Sign::positive);
    EXPECT_EQ(detail::compareY(b, detail::exactPoint({0.0, above(root)})), Sign::negative);
    const auto rounded = detail::roundedPoint(a);
    EXPECT_LE(std::fabs(rounded.x - root), 0x1p-51);
    EXPECT_EQ(rounded.y, 1.0);
}

TEST(Carriers, BeyondDomainIsExact) {
    using detail::beyondDomain;
    using detail::exactPoint;
    EXPECT_FALSE(beyondDomain(exactPoint({kMaxCoordinate, -kMaxCoordinate})));
    EXPECT_TRUE(beyondDomain(exactPoint({above(kMaxCoordinate), 0.0})));
    EXPECT_TRUE(beyondDomain(exactPoint({0.0, -above(kMaxCoordinate)})));
    EXPECT_FALSE(beyondDomain(exactPoint({0x1p-1074, 0.0})));
}

TEST(Carriers, SubnormalData) {
    // The circle of the smallest subnormal radius about the origin, and the
    // x axis: (-t, 0) and (t, 0) with t = 2^-1074.
    constexpr double kTiny = 0x1p-1074;
    const Curve circle = Circle{{0.0, 0.0}, kTiny};
    const Curve axis = Segment{{-1.0, 0.0}, {1.0, 0.0}};
    const auto meeting = meet(axis, circle);
    ASSERT_EQ(meeting.count, 2U);
    expectPoint(meeting.points[0].point, {-kTiny, 0.0});
    expectPoint(meeting.points[1].point, {kTiny, 0.0});
    expectOnBoth(axis, circle, meeting);
    const Curve above = Segment{{-1.0, kTiny}, {1.0, kTiny}};
    const auto touching = meet(circle, above);
    ASSERT_EQ(touching.count, 1U);
    EXPECT_EQ(touching.points[0].contact, Contact::touching);
    expectPoint(touching.points[0].point, {0.0, kTiny});
}

// --- Random pairs through a common point -----------------------------------

// SplitMix64: a deterministic stream, the same on every platform.
class Generator {
public:
    std::uint64_t next() {
        m_state += 0x9E3779B97F4A7C15ULL;
        std::uint64_t z = m_state;
        z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31U);
    }

    // An integer in [-range, range].
    std::int64_t integer(std::int64_t range) {
        const auto span = static_cast<std::uint64_t>(2 * range + 1);
        return static_cast<std::int64_t>(next() % span) - range;
    }

    double coordinate() { return static_cast<double>(integer(1024)) * 0x1p-4; }

private:
    std::uint64_t m_state = 0x50A9C4E2;
};

// A random curve whose carrier passes exactly through `through`.
Curve curveThrough(Generator& generator, Point2 through) {
    Point2 other{through.x + generator.coordinate(), through.y + generator.coordinate()};
    if (other.x == through.x && other.y == through.y) {
        other.x += 1.0;
    }
    // A Pythagorean offset (3k, 4k) puts `through` at distance 5k.
    const double k = static_cast<double>(1 + generator.integer(8) + 8) * 0x1p-3;
    const Point2 center{through.x - 3.0 * k, through.y - 4.0 * k};
    double bulge = static_cast<double>(generator.integer(40)) * 0x1p-4;
    if (bulge == 0.0) {
        bulge = 0.5;
    }
    switch (generator.next() % 5U) {
        case 0U:
            return Segment{through, other};
        case 1U:
            return PolylineSegment{through, other, 0.0};
        case 2U:
            return PolylineSegment{through, other, bulge};
        case 3U:
            return Circle{center, 5.0 * k};
        default:
            return Arc{center, 5.0 * k, generator.coordinate(),
                       1.0 + generator.coordinate() * 0x1p-8};
    }
}

TEST(Carriers, RandomPairsMeetAtTheirCommonPoint) {
    Generator generator;
    std::size_t sameCarriers = 0;
    for (int i = 0; i < 2000; ++i) {
        const Point2 through{generator.coordinate(), generator.coordinate()};
        const auto first = curveThrough(generator, through);
        const auto second = curveThrough(generator, through);
        const auto meeting = meet(first, second);
        expectOnBoth(first, second, meeting);
        expectSymmetric(first, second);
        if (meeting.sameCarrier) {
            ++sameCarriers;
            EXPECT_EQ(meeting.count, 0U);
            continue;
        }
        const auto common = detail::exactPoint(through);
        bool found = false;
        for (const auto& point : meeting.found()) {
            found = found || detail::samePoint(point.point, common);
        }
        EXPECT_TRUE(found) << "pair " << i;
        if (meeting.count == 2) {
            EXPECT_FALSE(detail::samePoint(meeting.points[0].point, meeting.points[1].point));
        }
        const bool twoLines = std::holds_alternative<detail::LineMeaning>(carrierOf(first)) &&
                              std::holds_alternative<detail::LineMeaning>(carrierOf(second));
        if (meeting.count == 1 && !twoLines) {
            EXPECT_EQ(meeting.points[0].contact, Contact::touching);
        } else {
            for (const auto& point : meeting.found()) {
                EXPECT_EQ(point.contact, Contact::crossing);
            }
        }
    }
    EXPECT_LT(sameCarriers, 200U);
}

}  // namespace
