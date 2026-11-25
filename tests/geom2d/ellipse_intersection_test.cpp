// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Properties of the two intersect() of ellipse.hpp (stage 0.3, package PS7),
// on data generated deterministically. The values of the delicate cases are in
// the corpus (references [0.3-RS7a], [0.3-RS7b] and [0.3-RS7c]).

#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "ellipse_conic.hpp"
#include "ellipse_meaning.hpp"
#include "exact/interval.hpp"
#include "exact/polynomial.hpp"
#include "positions.hpp"
#include "solid_space/geom2d/ellipse.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace {

namespace geom2d = solid_space::geom2d;
namespace detail = solid_space::geom2d::detail;
namespace exact = solid_space::geom2d::exact;
using exact::Dyadic;
using exact::Polynomial;
using geom2d::Circle;
using geom2d::Contact;
using geom2d::Ellipse;
using geom2d::EllipseIntersections;
using geom2d::Extension;
using geom2d::Location;
using geom2d::Point2;
using geom2d::Segment;
using solid_space::robust::Sign;

constexpr double kHalfTurn = geom2d::kFullTurn / 2.0;

// A small linear congruential generator: the same data on every system.
class Generator {
public:
    std::uint64_t next() {
        m_state = m_state * 6364136223846793005ULL + 1442695040888963407ULL;
        return m_state >> 33U;
    }

    // An integer in [-range, range], as a double.
    double coordinate(int range) {
        const auto width = 2 * static_cast<std::uint64_t>(range) + 1;
        return static_cast<double>(static_cast<std::int64_t>(next() % width) - range);
    }

    template <typename T, std::size_t N>
    const T& pick(const std::array<T, N>& values) {
        return values.at(next() % N);
    }

private:
    std::uint64_t m_state = 0x5eed;
};

// Ellipses with small exact data: axes with integer lengths, simple ratios,
// whole ellipses and arcs, so that tangencies and shared ends happen often.
Ellipse randomEllipse(Generator& random) {
    static constexpr std::array<Point2, 6> kAxes{
        {{5.0, 0.0}, {0.0, 5.0}, {3.0, 4.0}, {-4.0, 3.0}, {-5.0, 0.0}, {2.0, 0.0}}};
    static constexpr std::array<double, 4> kRatios{1.0, 0.5, 0.75, 0.25};
    static constexpr std::array<double, 7> kParameters{0.0, 0.0, 0.0, 1.0, 2.0, -1.0, 3.0};
    Ellipse ellipse;
    ellipse.center = {random.coordinate(3), random.coordinate(3)};
    ellipse.majorAxis = random.pick(kAxes);
    ellipse.ratio = random.pick(kRatios);
    ellipse.startParameter = random.pick(kParameters);
    ellipse.endParameter = random.pick(kParameters);
    return ellipse;
}

// Two rounded points of the same exact point.
bool close(Point2 a, Point2 b, double magnitude) {
    const auto near = [magnitude](double u, double v) {
        const double scale = std::max({magnitude, std::fabs(u), std::fabs(v)});
        return std::fabs(u - v) <= 2.0 * geom2d::kRelativePointError * scale;
    };
    return near(a.x, b.x) && near(a.y, b.y);
}

bool sameBits(Point2 a, Point2 b) {
    return std::bit_cast<std::uint64_t>(a.x) == std::bit_cast<std::uint64_t>(b.x) &&
           std::bit_cast<std::uint64_t>(a.y) == std::bit_cast<std::uint64_t>(b.y);
}

// The decisions of intersect(a, b) and intersect(b, a) are the same, with the
// two inputs swapped.
void expectSwappedDecisions(const EllipseIntersections& direct, const EllipseIntersections& swapped,
                            double magnitude) {
    ASSERT_EQ(direct.points.size(), swapped.points.size());
    ASSERT_EQ(direct.overlaps.size(), swapped.overlaps.size());
    EXPECT_EQ(direct.omittedBeyondDomain, swapped.omittedBeyondDomain);
    std::vector<bool> used(swapped.points.size(), false);
    for (const auto& point : direct.points) {
        bool found = false;
        for (std::size_t i = 0; i < swapped.points.size() && !found; ++i) {
            const auto& other = swapped.points[i];
            if (!used[i] && point.contact == other.contact && point.onFirst == other.onSecond &&
                point.onSecond == other.onFirst && close(point.point, other.point, magnitude)) {
                used[i] = true;
                found = true;
            }
        }
        EXPECT_TRUE(found) << "point (" << point.point.x << ", " << point.point.y << ")";
    }
    for (const auto& overlap : direct.overlaps) {
        const auto matches = [&](const geom2d::EllipseOverlap& other) {
            if (overlap.whole || other.whole) {
                return overlap.whole && other.whole;
            }
            const auto same = [&](const geom2d::EllipseOverlapEnd& u,
                                  const geom2d::EllipseOverlapEnd& v) {
                return close(u.point, v.point, magnitude) && u.onFirst == v.onSecond &&
                       u.onSecond == v.onFirst;
            };
            return (same(overlap.from, other.from) && same(overlap.to, other.to)) ||
                   (same(overlap.from, other.to) && same(overlap.to, other.from));
        };
        EXPECT_TRUE(std::ranges::any_of(swapped.overlaps, matches));
    }
}

TEST(EllipseIntersection, SwappingTwoEllipsesKeepsTheDecisions) {
    Generator random;
    std::size_t points = 0;
    std::size_t touching = 0;
    std::size_t overlaps = 0;
    for (int i = 0; i < 400; ++i) {
        const auto a = randomEllipse(random);
        auto b = randomEllipse(random);
        if (i % 4 == 0) {
            // The same carrier given another way: the opposite axis and the
            // parameters moved by about half a turn.
            b = a;
            b.majorAxis = {-a.majorAxis.x, -a.majorAxis.y};
            b.startParameter += kHalfTurn;
            b.endParameter += i % 8 == 0 ? kHalfTurn : 1.0;
        }
        for (const auto ea : {Extension::none, Extension::extended}) {
            for (const auto eb : {Extension::none, Extension::extended}) {
                const auto direct = geom2d::intersect(a, ea, b, eb);
                const auto swapped = geom2d::intersect(b, eb, a, ea);
                ASSERT_TRUE(direct.hasValue());
                ASSERT_TRUE(swapped.hasValue());
                const double magnitude =
                    std::max(geom2d::magnitude(a).value(), geom2d::magnitude(b).value());
                expectSwappedDecisions(direct.value(), swapped.value(), magnitude);
                points += direct.value().points.size();
                overlaps += direct.value().overlaps.size();
                touching += static_cast<std::size_t>(
                    std::ranges::count(direct.value().points, Contact::touching,
                                       &geom2d::EllipseIntersectionPoint::contact));
            }
        }
    }
    // The data reach every kind of answer.
    EXPECT_GT(points, 0U);
    EXPECT_GT(touching, 0U);
    EXPECT_GT(overlaps, 0U);
}

// The implicit equation A x^2 + B x y + C y^2 + D x + E y + F of a conic,
// expanded from its data apart from ellipse_conic.hpp, and its value at the
// rational form (x(s), y(s)) / weight(s), times weight^2.
struct Conic {
    Dyadic a, b, c, d, e, f;
};

Conic conicOf(const detail::EllipseSupport& support) {
    // (q x minor)^2 + (major x q)^2 - determinant^2, with q = p - centre.
    const auto& major = support.major;
    const auto& minor = support.minor;
    const auto& centre = support.center;
    const auto a = minor.y * minor.y + major.y * major.y;
    const auto b = -(minor.x * minor.y + major.x * major.y).timesPowerOfTwo(1);
    const auto c = minor.x * minor.x + major.x * major.x;
    return {a,
            b,
            c,
            -(a * centre.x).timesPowerOfTwo(1) - b * centre.y,
            -(c * centre.y).timesPowerOfTwo(1) - b * centre.x,
            a * centre.x * centre.x + b * centre.x * centre.y + c * centre.y * centre.y -
                support.determinant * support.determinant};
}

Conic conicOf(const Circle& circle) {
    const Dyadic x{circle.center.x};
    const Dyadic y{circle.center.y};
    const Dyadic r{circle.radius};
    const Dyadic one{1.0};
    return {
        one, Dyadic{}, one, -x.timesPowerOfTwo(1), -y.timesPowerOfTwo(1), x * x + y * y - r * r};
}

Polynomial along(const Conic& conic, const detail::RationalEllipse& form) {
    const auto& x = form.x;
    const auto& y = form.y;
    const auto& w = form.weight;
    return x * x * conic.a + x * y * conic.b + y * y * conic.c + x * w * conic.d + y * w * conic.e +
           w * w * conic.f;
}

Sign valueAt(const Conic& conic, Point2 point) {
    const Dyadic x{point.x};
    const Dyadic y{point.y};
    return (conic.a * x * x + conic.b * x * y + conic.c * y * y + conic.d * x + conic.e * y +
            conic.f)
        .sign();
}

// Every common point found lies exactly on both carriers.
void expectOnBoth(const Ellipse& ellipse, const Conic& other, const detail::ConicTrace& trace,
                  const detail::ArcMeaning& otherArc) {
    const auto meaning = detail::checkedEllipse(ellipse, 0).value();
    const auto form = detail::rationalForm(meaning.support);
    // The rational form lies on its own ellipse for every s.
    EXPECT_TRUE(along(conicOf(meaning.support), form).isZero());
    const auto equation = along(other, form);
    for (const auto& point : detail::meetEllipseConic(meaning, trace, otherArc)) {
        if (point.parameter) {
            EXPECT_EQ(exact::signAt(equation, *point.parameter), Sign::zero);
        } else {
            // P(infinity) = centre - major, a point of doubles here.
            const Point2 missing{ellipse.center.x - ellipse.majorAxis.x,
                                 ellipse.center.y - ellipse.majorAxis.y};
            EXPECT_EQ(valueAt(other, missing), Sign::zero);
            EXPECT_LT(equation.degree(), exact::kMaxDegree);
        }
    }
}

TEST(EllipseIntersection, EveryPointLiesExactlyOnBothCarriers) {
    Generator random;
    std::size_t checked = 0;
    for (int i = 0; i < 300; ++i) {
        const auto ellipse = randomEllipse(random);
        const auto meaning = detail::checkedEllipse(ellipse, 0).value();
        const auto form = detail::rationalForm(meaning.support);

        const auto otherEllipse = randomEllipse(random);
        const auto other = detail::checkedEllipse(otherEllipse, 1).value();
        const auto trace = detail::traceOfEllipse(form, other.support);
        if (!trace.equation.isZero()) {
            expectOnBoth(ellipse, conicOf(other.support), trace, other.unitArc);
            ++checked;
        }

        const Circle circle{{random.coordinate(3), random.coordinate(3)},
                            static_cast<double>(1 + random.next() % 6)};
        const auto curve = detail::meaningOf(circle);
        const auto& arc = std::get<detail::ArcMeaning>(curve.support);
        const auto circleTrace = detail::traceOfCircle(form, arc.circle);
        if (!circleTrace.equation.isZero()) {
            expectOnBoth(ellipse, conicOf(circle), circleTrace, arc);
            ++checked;
        }

        // The points of a line: exact points of the form (a + b sqrt(d)) / w.
        const Point2 start{random.coordinate(6), random.coordinate(6)};
        const Point2 end{random.coordinate(6), random.coordinate(6) + 0.5};
        const detail::LineMeaning line{start, end};
        const auto lineMeeting = detail::meetEllipseLine(meaning, line);
        for (const auto& point : lineMeeting.found()) {
            EXPECT_EQ(detail::ellipseSign(meaning.support, point.point), Sign::zero);
            EXPECT_EQ(detail::lineSign(line, point.point), Sign::zero);
        }
    }
    EXPECT_GT(checked, 0U);
}

// The box of a curved piece (stage 0.3 corrections, package FS4) skips only
// pairs without a common point: intersect() gives as many points as the exact
// meeting of the two carriers has points that take part, for pieces of every
// bulge, near the ellipse and far from it, each input extended or not.
TEST(EllipseIntersection, CurvedPiecesKeepEveryCommonPoint) {
    static constexpr std::array<double, 10> kBulges{0x1p-10, -0.01, 0.3,    -1.0,  1.0,
                                                    2.5,     -40.0, 1000.0, -0.75, 0.5};
    static constexpr std::array<double, 6> kShifts{0.0, 0.0, 6.0, -9.0, 20.0, -64.0};
    Generator random;
    std::size_t withPoints = 0;
    std::size_t withoutPoints = 0;
    for (int i = 0; i < 400; ++i) {
        const auto ellipse = randomEllipse(random);
        const double shiftX = random.pick(kShifts);
        const double shiftY = random.pick(kShifts);
        const geom2d::PolylineSegment piece{
            {random.coordinate(4) + shiftX, random.coordinate(4) + shiftY},
            {random.coordinate(4) + shiftX, random.coordinate(4) + shiftY},
            random.pick(kBulges)};
        if (piece.start.x == piece.end.x && piece.start.y == piece.end.y) {
            continue;
        }
        const auto meaning = detail::checkedEllipse(ellipse, 0).value();
        const auto form = detail::rationalForm(meaning.support);
        const auto pieceMeaning = detail::checkedMeaning(piece, 1);
        ASSERT_TRUE(pieceMeaning.hasValue());
        const auto& arc = std::get<detail::ArcMeaning>(pieceMeaning.value().support);
        const auto trace = detail::traceOfCircle(form, arc.circle);
        if (trace.equation.isZero()) {
            continue;
        }
        const auto points = detail::meetEllipseConic(meaning, trace, arc);
        for (const Extension ellipseExtension : {Extension::none, Extension::extended}) {
            for (const Extension pieceExtension : {Extension::none, Extension::extended}) {
                const auto expected = static_cast<std::size_t>(
                    std::count_if(points.begin(), points.end(), [&](const auto& point) {
                        return detail::takesPart(point.onEllipse, ellipseExtension) &&
                               detail::takesPart(point.onOther, pieceExtension);
                    }));
                const auto result =
                    geom2d::intersect(ellipse, ellipseExtension, piece, pieceExtension);
                ASSERT_TRUE(result.hasValue());
                EXPECT_EQ(result.value().points.size(), expected) << i;
                ++(expected > 0 ? withPoints : withoutPoints);
            }
        }
    }
    EXPECT_GT(withPoints, 0U);
    EXPECT_GT(withoutPoints, 0U);
}

// The points of an intersect(), bit for bit, with their contacts and
// locations, and the count of the omitted ones.
std::string textOf(const EllipseIntersections& result) {
    std::string text = std::to_string(result.omittedBeyondDomain) + ";";
    for (const auto& point : result.points) {
        text += std::to_string(std::bit_cast<std::uint64_t>(point.point.x)) + " " +
                std::to_string(std::bit_cast<std::uint64_t>(point.point.y)) + " " +
                std::to_string(static_cast<int>(point.contact)) + " " +
                std::to_string(static_cast<int>(point.onFirst)) + " " +
                std::to_string(static_cast<int>(point.onSecond)) + ";";
    }
    return text + std::to_string(result.overlaps.size());
}

// The floating-point filter of the quartic (stage 0.3 corrections, package
// FS4) changes no result: the same points, bit for bit, with the filter off
// and on, for ellipses, circles and curved pieces.
TEST(EllipseIntersection, TheFilterChangesNoResult) {
    static constexpr std::array<double, 5> kBulges{0.3, -1.0, 1.0, 2.5, -0.75};
    Generator random;
    std::size_t points = 0;
    for (int i = 0; i < 300; ++i) {
        const auto ellipse = randomEllipse(random);
        const auto other = randomEllipse(random);
        const geom2d::Curve circle = Circle{{random.coordinate(3), random.coordinate(3)},
                                            static_cast<double>(1 + random.next() % 6)};
        const geom2d::Curve piece =
            geom2d::PolylineSegment{{random.coordinate(4), random.coordinate(4)},
                                    {random.coordinate(4), random.coordinate(4)},
                                    random.pick(kBulges)};
        for (const Extension extension : {Extension::none, Extension::extended}) {
            std::array<std::string, 2> withEllipse;
            std::array<std::string, 2> withCircle;
            std::array<std::string, 2> withPiece;
            for (const bool enabled : {false, true}) {
                exact::enableFilter(enabled);
                const auto k = static_cast<std::size_t>(enabled);
                const auto a = geom2d::intersect(ellipse, extension, other, extension);
                const auto b = geom2d::intersect(ellipse, extension, circle, extension);
                const auto c = geom2d::intersect(ellipse, extension, piece, extension);
                withEllipse.at(k) = a.hasValue() ? textOf(a.value()) : "diagnosis";
                withCircle.at(k) = b.hasValue() ? textOf(b.value()) : "diagnosis";
                withPiece.at(k) = c.hasValue() ? textOf(c.value()) : "diagnosis";
                if (enabled && a.hasValue()) {
                    points += a.value().points.size();
                }
            }
            EXPECT_EQ(withEllipse[0], withEllipse[1]) << i;
            EXPECT_EQ(withCircle[0], withCircle[1]) << i;
            EXPECT_EQ(withPiece[0], withPiece[1]) << i;
        }
    }
    exact::enableFilter(true);
    EXPECT_GT(points, 0U);
}

TEST(EllipseIntersection, SegmentsGiveThePointsOfTheRectangleSides) {
    Generator random;
    for (int i = 0; i < 200; ++i) {
        const auto ellipse = randomEllipse(random);
        const auto meaning = detail::checkedEllipse(ellipse, 0).value();
        const auto exactStart =
            detail::ellipsePoint(meaning.support, meaning.unitArc.startDirection);
        const Point2 low{random.coordinate(8), random.coordinate(8)};
        // A rectangle, or one reduced to a segment.
        const double width = i % 10 == 0 ? 0.0 : static_cast<double>(random.next() % 7);
        const double height = i % 10 == 5 ? 0.0 : static_cast<double>(random.next() % 7);
        const Point2 high{low.x + width, low.y + height};
        const Point2 upperLeft{low.x, high.y};
        const Point2 lowerRight{high.x, low.y};
        std::vector<Segment> sides;
        if (width == 0.0 || height == 0.0) {
            if (width != 0.0 || height != 0.0) {
                sides.push_back(Segment{low, high});
            }
        } else {
            sides = {Segment{low, upperLeft}, Segment{upperLeft, high}, Segment{high, lowerRight},
                     Segment{lowerRight, low}};
        }
        if (sides.empty()) {
            continue;
        }
        bool meetsSide = false;
        for (const auto& side : sides) {
            const auto result = geom2d::intersect(ellipse, Extension::none, side, Extension::none);
            ASSERT_TRUE(result.hasValue());
            const auto& found = result.value();
            EXPECT_TRUE(found.overlaps.empty());
            meetsSide = meetsSide || !found.points.empty();
            // The same exact points as the meeting that meetsRectangle reads.
            std::vector<Point2> expected;
            const auto meeting = detail::meetEllipseLine(meaning, {side.start, side.end});
            for (const auto& hit : meeting.found()) {
                if (detail::takesPart(hit.onEllipse, Extension::none) &&
                    detail::takesPart(hit.onSegment, Extension::none)) {
                    expected.push_back(detail::nearestPoint(hit.point));
                }
            }
            ASSERT_EQ(found.points.size(), expected.size());
            for (const auto& point : found.points) {
                EXPECT_TRUE(std::ranges::any_of(
                    expected, [&point](Point2 other) { return sameBits(point.point, other); }));
            }
        }
        const auto minimum = detail::exactPoint(low);
        const auto maximum = detail::exactPoint(high);
        const bool startInside = detail::compareX(exactStart, minimum) != Sign::negative &&
                                 detail::compareX(exactStart, maximum) != Sign::positive &&
                                 detail::compareY(exactStart, minimum) != Sign::negative &&
                                 detail::compareY(exactStart, maximum) != Sign::positive;
        const auto meets = geom2d::meetsRectangle(ellipse, low, high);
        ASSERT_TRUE(meets.hasValue());
        EXPECT_EQ(meets.value(), startInside || meetsSide) << "case " << i;
    }
}

TEST(EllipseIntersection, EllipseOfRatioOneSharesItsWholeCircle) {
    for (const Point2 axis : {Point2{5.0, 0.0}, Point2{3.0, 4.0}, Point2{0.0, -5.0}}) {
        const Ellipse ellipse{{1.0, 2.0}, axis, 1.0, 0.7, 0.7};
        const auto result =
            geom2d::intersect(ellipse, Extension::none, Circle{{1.0, 2.0}, 5.0}, Extension::none);
        ASSERT_TRUE(result.hasValue());
        const auto& found = result.value();
        EXPECT_TRUE(found.points.empty());
        ASSERT_EQ(found.overlaps.size(), 1U);
        EXPECT_TRUE(found.overlaps[0].whole);
        EXPECT_EQ(found.overlaps[0].from.onFirst, Location::atStart);
        EXPECT_EQ(found.overlaps[0].from.onSecond, Location::inside);
    }
}

TEST(EllipseIntersection, EllipseOfRatioOneSharesAnArcFromItsStartToItsEnd) {
    const Ellipse ellipse{{0.0, 0.0}, {5.0, 0.0}, 1.0, 0.0, 0.0};
    const geom2d::Arc arc{{0.0, 0.0}, 5.0, 1.0, -0.5};
    const auto result = geom2d::intersect(ellipse, Extension::none, arc, Extension::none);
    ASSERT_TRUE(result.hasValue());
    ASSERT_EQ(result.value().overlaps.size(), 1U);
    const auto& overlap = result.value().overlaps[0];
    EXPECT_FALSE(overlap.whole);
    // The arc runs clockwise: along the ellipse it goes from its end to its start.
    EXPECT_EQ(overlap.from.onSecond, Location::atEnd);
    EXPECT_EQ(overlap.to.onSecond, Location::atStart);
    EXPECT_EQ(overlap.from.onFirst, Location::inside);
    EXPECT_EQ(overlap.to.onFirst, Location::inside);
}

TEST(EllipseIntersection, PointMissedByTheRationalFormIsFound) {
    // centre - major = (-10, 0): a circle through it, crossing; one tangent
    // there; and the circle of curvature of the vertex (radius b^2 / a = 2.5),
    // a contact of order four: one touching point.
    const Ellipse ellipse{{0.0, 0.0}, {10.0, 0.0}, 0.5, 0.0, 0.0};
    const auto crossing =
        geom2d::intersect(ellipse, Extension::none, Circle{{-10.0, 5.0}, 5.0}, Extension::none);
    ASSERT_TRUE(crossing.hasValue());
    ASSERT_EQ(crossing.value().points.size(), 2U);
    EXPECT_EQ(crossing.value().points[1].point.x, -10.0);
    EXPECT_EQ(crossing.value().points[1].contact, Contact::crossing);

    for (const auto& circle : {Circle{{-20.0, 0.0}, 10.0}, Circle{{-7.5, 0.0}, 2.5}}) {
        const auto touching = geom2d::intersect(ellipse, Extension::none, circle, Extension::none);
        ASSERT_TRUE(touching.hasValue());
        ASSERT_EQ(touching.value().points.size(), 1U);
        const auto& point = touching.value().points[0];
        EXPECT_EQ(point.point.x, -10.0);
        EXPECT_EQ(point.point.y, 0.0);
        EXPECT_EQ(point.contact, Contact::touching);
        EXPECT_EQ(point.onFirst, Location::inside);
    }
}

TEST(EllipseIntersection, PointsBeyondTheArcFollowWhenExtended) {
    // The upper half of the ellipse, and the line x = 0 through (0, 5) and
    // (0, -5): the second point comes after the end of the arc.
    const Ellipse ellipse{{0.0, 0.0}, {10.0, 0.0}, 0.5, 0.0, kHalfTurn};
    const Segment segment{{0.0, -10.0}, {0.0, 10.0}};
    const auto none = geom2d::intersect(ellipse, Extension::none, segment, Extension::none);
    ASSERT_TRUE(none.hasValue());
    ASSERT_EQ(none.value().points.size(), 1U);
    EXPECT_EQ(none.value().points[0].point.y, 5.0);
    const auto extended = geom2d::intersect(ellipse, Extension::extended, segment, Extension::none);
    ASSERT_TRUE(extended.hasValue());
    ASSERT_EQ(extended.value().points.size(), 2U);
    EXPECT_EQ(extended.value().points[0].point.y, 5.0);
    EXPECT_EQ(extended.value().points[1].point.y, -5.0);
    EXPECT_EQ(extended.value().points[1].onFirst, Location::afterEnd);
}

TEST(EllipseIntersection, DiagnosesTheEllipseFirst) {
    const Ellipse bad{{0.0, 0.0}, {0.0, 0.0}};
    const Ellipse good{{0.0, 0.0}, {1.0, 0.0}};
    const auto withCurve =
        geom2d::intersect(bad, Extension::none, Segment{{0.0, 0.0}, {0.0, 0.0}}, Extension::none);
    ASSERT_FALSE(withCurve.hasValue());
    EXPECT_EQ(withCurve.diagnosis().code, geom2d::DiagnosisCode::degenerateCurve);
    EXPECT_EQ(withCurve.diagnosis().input, 0U);
    const auto withEllipse = geom2d::intersect(good, Extension::none, bad, Extension::none);
    ASSERT_FALSE(withEllipse.hasValue());
    EXPECT_EQ(withEllipse.diagnosis().input, 1U);
}

TEST(EllipseIntersection, SwappingReportsTheSameBadEllipse) {
    // Only the second ellipse is bad: swapped, the same code names input 0.
    const Ellipse good{{0.0, 0.0}, {10.0, 0.0}, 0.5};
    const Ellipse negativeRatio{{0.0, 0.0}, {10.0, 0.0}, -1.0};
    const auto direct = geom2d::intersect(good, Extension::none, negativeRatio, Extension::none);
    const auto swapped = geom2d::intersect(negativeRatio, Extension::none, good, Extension::none);
    ASSERT_FALSE(direct.hasValue());
    ASSERT_FALSE(swapped.hasValue());
    EXPECT_EQ(direct.diagnosis().code, geom2d::DiagnosisCode::invalidValue);
    EXPECT_EQ(direct.diagnosis().input, 1U);
    EXPECT_EQ(swapped.diagnosis().code, geom2d::DiagnosisCode::invalidValue);
    EXPECT_EQ(swapped.diagnosis().input, 0U);

    // Both are bad: each call reports the one given first.
    const Ellipse farCenter{{1.0e300, 0.0}, {10.0, 0.0}, 0.5};
    const auto both = geom2d::intersect(farCenter, Extension::none, negativeRatio, Extension::none);
    const auto bothSwapped =
        geom2d::intersect(negativeRatio, Extension::none, farCenter, Extension::none);
    ASSERT_FALSE(both.hasValue());
    ASSERT_FALSE(bothSwapped.hasValue());
    EXPECT_EQ(both.diagnosis().code, geom2d::DiagnosisCode::outOfDomain);
    EXPECT_EQ(both.diagnosis().input, 0U);
    EXPECT_EQ(bothSwapped.diagnosis().code, geom2d::DiagnosisCode::invalidValue);
    EXPECT_EQ(bothSwapped.diagnosis().input, 0U);
}

}  // namespace
