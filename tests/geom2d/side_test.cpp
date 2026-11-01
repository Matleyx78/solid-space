// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Unit tests of side() (stage 0.2, package P9). The expected sides follow
// from side.hpp and from exact arithmetic done by hand in the comments; the
// cases of [0.2-R9] in tests/corpus/ cover the request and its limits.

#include "solid_space/geom2d/side.hpp"

#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "solid_space/robust/predicates.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <variant>

namespace {

using solid_space::geom2d::Arc;
using solid_space::geom2d::Circle;
using solid_space::geom2d::Curve;
using solid_space::geom2d::DiagnosisCode;
using solid_space::geom2d::Point2;
using solid_space::geom2d::PolylineSegment;
using solid_space::geom2d::Segment;
using solid_space::geom2d::Side;
using solid_space::geom2d::side;
using solid_space::robust::Sign;

namespace detail = solid_space::geom2d::detail;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kMaxCoordinate = 0x1p50;
constexpr double kTiny = 0x1p-1074;

double below(double value) {
    return std::nextafter(value, -kInfinity);
}

double above(double value) {
    return std::nextafter(value, kInfinity);
}

void expectSide(const Curve& curve, Point2 point, Side expected) {
    const auto result = side(curve, point);
    ASSERT_TRUE(result.hasValue()) << result.diagnosis().message;
    EXPECT_EQ(result.value(), expected) << point.x << ", " << point.y;
}

void expectDiagnosis(const Curve& curve, Point2 point, DiagnosisCode code, std::size_t input) {
    const auto result = side(curve, point);
    ASSERT_FALSE(result.hasValue());
    EXPECT_EQ(result.diagnosis().code, code);
    EXPECT_EQ(result.diagnosis().input, input);
    EXPECT_FALSE(result.diagnosis().message.empty());
}

TEST(Side, StraightCurves) {
    for (const Curve& curve : {Curve{Segment{{0.0, 0.0}, {4.0, 2.0}}},
                               Curve{PolylineSegment{{0.0, 0.0}, {4.0, 2.0}, 0.0}},
                               Curve{PolylineSegment{{0.0, 0.0}, {4.0, 2.0}, -0.0}}}) {
        expectSide(curve, {2.0, 1.0}, Side::on);
        expectSide(curve, {-6.0, -3.0}, Side::on);  // on the extension
        expectSide(curve, {2.0, above(1.0)}, Side::left);
        expectSide(curve, {2.0, below(1.0)}, Side::right);
        expectSide(curve, {0.0, kTiny}, Side::left);
        expectSide(curve, {0.0, -kTiny}, Side::right);
    }
    // Reversed, left and right swap.
    expectSide(Segment{{4.0, 2.0}, {0.0, 0.0}}, {2.0, above(1.0)}, Side::right);
}

TEST(Side, TinyAndLargeCoordinates) {
    // A segment of subnormal length and a point one subnormal off it.
    const Curve tiny = Segment{{0.0, 0.0}, {2.0 * kTiny, kTiny}};
    expectSide(tiny, {4.0 * kTiny, 2.0 * kTiny}, Side::on);
    expectSide(tiny, {4.0 * kTiny, 3.0 * kTiny}, Side::left);
    expectSide(tiny, {4.0 * kTiny, kTiny}, Side::right);
    // Data at the edge of the domain.
    const Curve large =
        Segment{{-kMaxCoordinate, -kMaxCoordinate}, {kMaxCoordinate, kMaxCoordinate}};
    expectSide(large, {1.0, 1.0}, Side::on);
    expectSide(large, {below(kMaxCoordinate), kMaxCoordinate}, Side::left);
}

TEST(Side, CirclesAndArcs) {
    // (3, 4) is on the circle of radius 5 about the origin.
    const Point2 on{3.0, 4.0};
    const Point2 in{3.0, below(4.0)};
    const Point2 out{3.0, above(4.0)};
    const Curve circle = Circle{{0.0, 0.0}, 5.0};
    expectSide(circle, on, Side::on);
    expectSide(circle, in, Side::left);
    expectSide(circle, out, Side::right);
    expectSide(circle, {0.0, 0.0}, Side::left);
    // Counterclockwise arcs: inside is left, whatever D0 and D1 are.
    for (const double sweep : {1e-3, 1.0, 3.0, 6.0, 7.0}) {
        const Curve arc = Arc{{0.0, 0.0}, 5.0, -2.0, sweep};
        expectSide(arc, on, Side::on);
        expectSide(arc, in, Side::left);
        expectSide(arc, out, Side::right);
    }
    // Clockwise arcs: inside is right.
    for (const double sweep : {-1e-3, -1.0, -3.0, -6.0, -7.0}) {
        const Curve arc = Arc{{0.0, 0.0}, 5.0, 0.5, sweep};
        expectSide(arc, on, Side::on);
        expectSide(arc, in, Side::right);
        expectSide(arc, out, Side::left);
    }
    // A point on the rest of the circle of an arc is on too.
    expectSide(Arc{{0.0, 0.0}, 5.0, 0.0, 0.1}, {-3.0, -4.0}, Side::on);
}

TEST(Side, CurvedPieces) {
    // (-5, 0) -> (5, 0): bulge 1 runs counterclockwise below the axis, -1
    // clockwise above it; both lie on the circle of radius 5 about the origin.
    const Curve counterclockwise = PolylineSegment{{-5.0, 0.0}, {5.0, 0.0}, 1.0};
    const Curve clockwise = PolylineSegment{{-5.0, 0.0}, {5.0, 0.0}, -1.0};
    expectSide(counterclockwise, {3.0, 4.0}, Side::on);
    expectSide(counterclockwise, {3.0, below(4.0)}, Side::left);
    expectSide(counterclockwise, {3.0, above(4.0)}, Side::right);
    expectSide(clockwise, {-3.0, -4.0}, Side::on);
    expectSide(clockwise, {0.0, 0.0}, Side::right);
    expectSide(clockwise, {0.0, above(5.0)}, Side::left);
    // A small bulge: (0, 0) -> (2, 0) with bulge 1/4. m = (1, -1/4), and the
    // circle through (0, 0), (1, -1/4), (2, 0) has centre (1, 15/8) and
    // radius 17/8: (1, 4) is on it, (1, 15/8) inside, which is left.
    const Curve shallow = PolylineSegment{{0.0, 0.0}, {2.0, 0.0}, 0.25};
    expectSide(shallow, {1.0, 4.0}, Side::on);
    expectSide(shallow, {1.0, 1.875}, Side::left);
    expectSide(shallow, {1.0, above(4.0)}, Side::right);
    expectSide(shallow, {1.0, below(-0.25)}, Side::right);
    expectSide(shallow, {1.0, -0.25}, Side::on);
}

TEST(Side, DiagnosesAboutTheCurve) {
    const Point2 point{1.0, 1.0};
    expectDiagnosis(Segment{{kNaN, 0.0}, {1.0, 0.0}}, point, DiagnosisCode::invalidValue, 0);
    expectDiagnosis(Circle{{0.0, 0.0}, -1.0}, point, DiagnosisCode::invalidValue, 0);
    expectDiagnosis(Arc{{0.0, 0.0}, 1.0, kInfinity, 1.0}, point, DiagnosisCode::invalidValue, 0);
    expectDiagnosis(Segment{{0.0, 0.0}, {above(kMaxCoordinate), 0.0}}, point,
                    DiagnosisCode::outOfDomain, 0);
    expectDiagnosis(Segment{{1.0, 2.0}, {1.0, 2.0}}, point, DiagnosisCode::degenerateCurve, 0);
    expectDiagnosis(Circle{{0.0, 0.0}, 0.0}, point, DiagnosisCode::degenerateCurve, 0);
    expectDiagnosis(Arc{{0.0, 0.0}, 1.0, 0.5, 0.0}, point, DiagnosisCode::degenerateCurve, 0);
    expectDiagnosis(PolylineSegment{{1.0, 2.0}, {1.0, 2.0}, 1.0}, point,
                    DiagnosisCode::degenerateCurve, 0);
}

TEST(Side, DiagnosesAboutThePoint) {
    const Curve curve = Circle{{0.0, 0.0}, 1.0};
    expectDiagnosis(curve, {kNaN, 0.0}, DiagnosisCode::invalidValue, 1);
    expectDiagnosis(curve, {0.0, -kInfinity}, DiagnosisCode::invalidValue, 1);
    expectDiagnosis(curve, {above(kMaxCoordinate), 0.0}, DiagnosisCode::outOfDomain, 1);
    expectDiagnosis(curve, {0.0, -above(kMaxCoordinate)}, DiagnosisCode::outOfDomain, 1);
    expectSide(curve, {kMaxCoordinate, -kMaxCoordinate}, Side::right);
    // The curve is checked first.
    expectDiagnosis(Segment{{0.0, 0.0}, {0.0, 0.0}}, {kNaN, 0.0}, DiagnosisCode::degenerateCurve,
                    0);
}

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

    std::int64_t integer(std::int64_t range) {
        const auto span = static_cast<std::uint64_t>(2 * range + 1);
        return static_cast<std::int64_t>(next() % span) - range;
    }

    double coordinate() { return static_cast<double>(integer(1024)) * 0x1p-4; }

private:
    std::uint64_t m_state = 0x51DE51DE;
};

TEST(Side, AgreesWithTheCarrierSign) {
    // On random points near random curves, side() and the sign of carriers.hpp
    // are the same comparison: on exactly when the exact point is on the
    // carrier.
    Generator generator;
    for (int i = 0; i < 3000; ++i) {
        const Point2 a{generator.coordinate(), generator.coordinate()};
        Point2 b{generator.coordinate(), generator.coordinate()};
        if (a.x == b.x && a.y == b.y) {
            b.x += 1.0;
        }
        const double radius = std::fabs(generator.coordinate()) + 0.5;
        const Curve curves[] = {Segment{a, b}, PolylineSegment{a, b, 0.75},
                                PolylineSegment{a, b, -0.125}, Circle{a, radius},
                                Arc{a, radius, 1.0, -2.0}};
        // A point on the segment from a to b, rounded: often exactly on, often
        // one rounding away.
        const double t = static_cast<double>(generator.integer(16)) / 3.0;
        const Point2 points[] = {{a.x + t * (b.x - a.x), a.y + t * (b.y - a.y)},
                                 {a.x + radius, a.y},
                                 {generator.coordinate(), generator.coordinate()}};
        for (const auto& curve : curves) {
            const auto meaning = detail::meaningOf(curve);
            const auto carrier = detail::carrierOf(meaning);
            for (const auto& point : points) {
                const auto result = side(curve, point);
                ASSERT_TRUE(result.hasValue());
                const auto exact = detail::exactPoint(point);
                const auto sign = detail::sideSign(meaning, exact);
                const Side expected = sign == Sign::positive   ? Side::left
                                      : sign == Sign::negative ? Side::right
                                                               : Side::on;
                EXPECT_EQ(result.value(), expected);
                EXPECT_EQ(result.value() == Side::on, detail::onCarrier(carrier, exact));
                if (const auto* line = std::get_if<detail::LineMeaning>(&meaning.support)) {
                    EXPECT_EQ(detail::lineSign(*line, exact),
                              solid_space::robust::orient2d(line->start, line->end, point));
                }
            }
        }
    }
}

// A random curve whose carrier passes exactly through `through` (as in
// carriers_test.cpp).
Curve curveThrough(Generator& generator, Point2 through) {
    Point2 other{through.x + generator.coordinate(), through.y + generator.coordinate()};
    if (other.x == through.x && other.y == through.y) {
        other.x += 1.0;
    }
    const double k = static_cast<double>(9 + generator.integer(8)) * 0x1p-3;
    const Point2 center{through.x - 3.0 * k, through.y - 4.0 * k};
    double bulge = static_cast<double>(generator.integer(40)) * 0x1p-4;
    if (bulge == 0.0) {
        bulge = -0.5;
    }
    switch (generator.next() % 4U) {
        case 0U:
            return Segment{through, other};
        case 1U:
            return PolylineSegment{through, other, bulge};
        case 2U:
            return Circle{center, 5.0 * k};
        default:
            return Arc{center, 5.0 * k, generator.coordinate(), -1.5};
    }
}

TEST(Side, OnForEveryMeetingPointThatIsADouble) {
    // Request #20, case 1, and the points of a line or a circle with a circle
    // that are doubles.
    const Curve x1 = Segment{{0.0, 0.0}, {10.0, 10.0}};
    const Curve x2 = Segment{{0.0, 10.0}, {10.0, 0.0}};
    const Curve circle = Circle{{0.0, 0.0}, 5.0};
    const Curve other = PolylineSegment{{6.0, 5.0}, {6.0, -5.0}, 1.0};
    const Curve line = Segment{{-7.0, 4.0}, {7.0, 4.0}};
    std::size_t checked = 0;
    const auto check = [&](const Curve& first, const Curve& second) {
        const auto meeting = detail::meet(detail::carrierOf(detail::meaningOf(first)),
                                          detail::carrierOf(detail::meaningOf(second)));
        for (const auto& found : meeting.found()) {
            const auto rounded = detail::roundedPoint(found.point);
            if (!detail::samePoint(found.point, detail::exactPoint(rounded))) {
                continue;
            }
            ++checked;
            expectSide(first, rounded, Side::on);
            expectSide(second, rounded, Side::on);
        }
    };
    check(x1, x2);
    check(circle, other);
    check(circle, line);
    check(line, other);
    // (5, 5); (3, -4) and (3, 4); (-3, 4) and (3, 4); (3, 4) and (9, 4).
    EXPECT_EQ(checked, 7U);

    // Random curves through a common double point: the meeting finds it, and
    // side() is on for both curves there.
    Generator generator;
    checked = 0;
    for (int i = 0; i < 2000; ++i) {
        const Point2 through{generator.coordinate(), generator.coordinate()};
        check(curveThrough(generator, through), curveThrough(generator, through));
    }
    EXPECT_GT(checked, 1500U);
}

}  // namespace
