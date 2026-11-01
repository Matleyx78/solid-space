// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Unit tests of the positions along the curves (stage 0.2, package P10).
// Correct rounding is checked against the square root of IEEE 754, which is
// correctly rounded, and against midpoints written by hand; the stations
// against an independent long double computation (std::atan2 is fine in a
// test: only the library must be deterministic), within the declared error;
// the order of the stations against the exact order on points generated in a
// fixed way, also across the bounds of the eighths of a turn.

#include "positions.hpp"

#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "exact/dyadic.hpp"
#include "solid_space/geom2d/curves.hpp"
#include "solid_space/geom2d/intersection.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <vector>

namespace {

using solid_space::geom2d::Arc;
using solid_space::geom2d::Circle;
using solid_space::geom2d::Curve;
using solid_space::geom2d::kRelativeStationError;
using solid_space::geom2d::Location;
using solid_space::geom2d::Point2;
using solid_space::geom2d::PolylineSegment;
using solid_space::geom2d::Segment;
using solid_space::geom2d::detail::CurveMeaning;
using solid_space::geom2d::detail::Dyadic;
using solid_space::geom2d::detail::ExactPoint;
using solid_space::geom2d::detail::ExactVector;
using solid_space::robust::Sign;

namespace detail = solid_space::geom2d::detail;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

bool sameBits(double a, double b) {
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

double next(double value) {
    return std::nextafter(value, kInfinity);
}

double previous(double value) {
    return std::nextafter(value, -kInfinity);
}

// A fixed pseudo-random sequence (splitmix64): the same numbers everywhere.
class Sequence {
public:
    explicit Sequence(std::uint64_t seed) : m_state(seed) {}

    std::uint64_t bits() {
        m_state += 0x9e3779b97f4a7c15ULL;
        std::uint64_t z = m_state;
        z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9ULL;
        z = (z ^ (z >> 27U)) * 0x94d049bb133111ebULL;
        return z ^ (z >> 31U);
    }

    // A double in [-limit, limit] with 30 random bits.
    double between(double limit) {
        const auto value = static_cast<double>(bits() >> 34U);
        return limit * ((value / 0x1p29) - 1.0);
    }

private:
    std::uint64_t m_state;
};

CurveMeaning meaningOf(const Curve& curve) {
    const auto meaning = detail::meaningOf(curve);
    EXPECT_FALSE(meaning.degenerate);
    return meaning;
}

// The exact points where the line through a and b meets the carrier of the
// curve.
std::vector<ExactPoint> pointsOn(const CurveMeaning& curve, Point2 a, Point2 b) {
    const auto meeting =
        detail::meet(detail::carrierOf(curve), detail::Carrier{detail::LineMeaning{a, b}});
    std::vector<ExactPoint> points;
    for (const auto& found : meeting.found()) {
        points.push_back(found.point);
    }
    return points;
}

// --- Correct rounding ------------------------------------------------------

double rounded(double a, double b, double d, double w, double k) {
    return detail::correctlyRounded(Dyadic(a), Dyadic(b), Dyadic(d), Dyadic(w), Dyadic(k));
}

TEST(CorrectlyRounded, SquareRootsMatchIeee754) {
    Sequence sequence(1);
    for (int i = 0; i < 2000; ++i) {
        const double d = std::ldexp(std::fabs(sequence.between(1.0)) + 0x1p-40,
                                    static_cast<int>(sequence.bits() % 200U) - 100);
        EXPECT_TRUE(sameBits(rounded(0.0, 1.0, d, 1.0, 1.0), std::sqrt(d))) << d;
        // A power of two in w only moves the exponent.
        EXPECT_TRUE(sameBits(rounded(0.0, 1.0, d, 8.0, 1.0), std::sqrt(d) / 8.0)) << d;
        EXPECT_TRUE(sameBits(rounded(0.0, -1.0, d, 1.0, 1.0), -std::sqrt(d))) << d;
        // sqrt(d) / sqrt(k) with d = k x^2 is x exactly.
        const double x = sequence.between(4.0);
        const double k = std::fabs(sequence.between(8.0)) + 1.0;
        const auto product = Dyadic(k) * Dyadic(x) * Dyadic(x);
        if (x != 0.0) {
            EXPECT_TRUE(sameBits(detail::correctlyRounded(Dyadic(), Dyadic(x > 0.0 ? 1.0 : -1.0),
                                                          product, Dyadic(1.0), Dyadic(k)),
                                 x));
        }
        // sqrt(d) / sqrt(k) = sqrt(d / k): both correctly rounded when d / k
        // is a double.
        EXPECT_TRUE(sameBits(rounded(0.0, 1.0, d * 4.0, 1.0, 4.0), std::sqrt(d))) << d;
    }
}

TEST(CorrectlyRounded, RationalsAndZeros) {
    EXPECT_TRUE(sameBits(rounded(1.0, 0.0, 0.0, 3.0, 1.0), 1.0 / 3.0));
    EXPECT_TRUE(sameBits(rounded(-2.0, 0.0, 0.0, 3.0, 1.0), -2.0 / 3.0));
    // -3 + sqrt(9) is an exact zero: +0, whatever the arithmetic gives.
    EXPECT_TRUE(sameBits(rounded(-3.0, 1.0, 9.0, 1.0, 1.0), 0.0));
    EXPECT_TRUE(sameBits(rounded(-0.0, 0.0, 0.0, 1.0, 1.0), 0.0));
    EXPECT_TRUE(sameBits(rounded(-0.0, 0.0, 0.0, 1.0, 2.0), 0.0));
}

TEST(CorrectlyRounded, TiesGoToEven) {
    // 1 + 2^-53 is the midpoint of 1 and 1 + 2^-52: even is 1.
    EXPECT_TRUE(sameBits(rounded(1.0, 0x1p-53, 1.0, 1.0, 1.0), 1.0));
    // 1 + 3 * 2^-53 is the midpoint of 1 + 2^-52 and 1 + 2^-51: even is the
    // second.
    EXPECT_TRUE(sameBits(rounded(1.0, 3 * 0x1p-53, 1.0, 1.0, 1.0), 1.0 + 0x1p-51));
    // The same midpoint through a radical over sqrt(k): (2 + 2^-52) / sqrt(4).
    EXPECT_TRUE(sameBits(rounded(2.0, 0x1p-52, 1.0, 1.0, 4.0), 1.0));
    EXPECT_TRUE(sameBits(rounded(-1.0, -0x1p-53, 1.0, 1.0, 1.0), -1.0));
}

TEST(CorrectlyRounded, NeverDecreasesAcrossMidpoints) {
    // Values just below, at and just above the midpoint of 1 and next(1),
    // written with a radical: 1 + 2^-53 + e sqrt(2) for tiny e.
    const auto middle = Dyadic(1.0) + Dyadic(0x1p-53);
    std::vector<double> results;
    for (const double e : {-0x1p-200, -0x1p-400, 0.0, 0x1p-400, 0x1p-200}) {
        results.push_back(
            detail::correctlyRounded(middle, Dyadic(e), Dyadic(2.0), Dyadic(1.0), Dyadic(1.0)));
    }
    EXPECT_TRUE(sameBits(results[0], 1.0));
    EXPECT_TRUE(sameBits(results[1], 1.0));
    EXPECT_TRUE(sameBits(results[2], 1.0));
    EXPECT_TRUE(sameBits(results[3], next(1.0)));
    EXPECT_TRUE(sameBits(results[4], next(1.0)));
    EXPECT_TRUE(std::is_sorted(results.begin(), results.end()));
}

// --- Locations ---------------------------------------------------------------

TEST(Locate, StraightCurve) {
    const auto curve = meaningOf(Segment{{0.0, 0.0}, {4.0, 0.0}});
    const auto at = [&curve](double x) {
        return detail::locate(curve, detail::exactPoint({x, 0.0}));
    };
    EXPECT_EQ(at(-1.0), Location::beforeStart);
    EXPECT_EQ(at(-0.0), Location::atStart);
    EXPECT_EQ(at(0x1p-1074), Location::inside);
    EXPECT_EQ(at(previous(4.0)), Location::inside);
    EXPECT_EQ(at(4.0), Location::atEnd);
    EXPECT_EQ(at(next(4.0)), Location::afterEnd);
}

TEST(Locate, CircularCurves) {
    // The quarter of the unit circle from (1, 0) to (0, 1), as a polyline
    // segment (bulge tan(pi / 8) is not a double, but any bulge in (0, 1)
    // gives an arc that the rational points below test exactly).
    const PolylineSegment quarter{{1.0, 0.0}, {0.0, 1.0}, std::numbers::sqrt2 - 1.0};
    const auto piece = meaningOf(quarter);
    // Points of its circle on the axes and on the diagonals.
    const auto onDiagonal = pointsOn(piece, {0.0, 0.0}, {1.0, 1.0});
    ASSERT_EQ(onDiagonal.size(), 2U);
    const auto start = detail::startPoint(piece);
    const auto end = detail::endPoint(piece);
    EXPECT_EQ(detail::locate(piece, start), Location::atStart);
    EXPECT_EQ(detail::locate(piece, end), Location::atEnd);

    const auto circle = meaningOf(Circle{{0.0, 0.0}, 1.0});
    EXPECT_EQ(detail::locate(circle, detail::exactPoint({1.0, 0.0})), Location::inside);
    EXPECT_EQ(detail::locate(circle, detail::exactPoint({0.0, -1.0})), Location::inside);

    // A whole-circle arc: only its start is atStart.
    const auto whole = meaningOf(Arc{{0.0, 0.0}, 1.0, 0.0, 7.0});
    EXPECT_EQ(detail::locate(whole, detail::exactPoint({1.0, 0.0})), Location::atStart);
    EXPECT_EQ(detail::locate(whole, detail::exactPoint({-1.0, 0.0})), Location::inside);

    // A half circle, clockwise from (1, 0) through (0, -1) to (-1, 0).
    const auto half = meaningOf(PolylineSegment{{1.0, 0.0}, {-1.0, 0.0}, -1.0});
    EXPECT_EQ(detail::locate(half, detail::exactPoint({1.0, 0.0})), Location::atStart);
    EXPECT_EQ(detail::locate(half, detail::exactPoint({0.0, -1.0})), Location::inside);
    EXPECT_EQ(detail::locate(half, detail::exactPoint({-1.0, 0.0})), Location::atEnd);
    EXPECT_EQ(detail::locate(half, detail::exactPoint({0.0, 1.0})), Location::afterEnd);
}

TEST(Locate, TakesPart) {
    using solid_space::geom2d::Extension;
    for (const auto location : {Location::atStart, Location::inside, Location::atEnd}) {
        EXPECT_TRUE(detail::takesPart(location, Extension::none));
        EXPECT_TRUE(detail::takesPart(location, Extension::extended));
    }
    for (const auto location : {Location::beforeStart, Location::afterEnd}) {
        EXPECT_FALSE(detail::takesPart(location, Extension::none));
        EXPECT_TRUE(detail::takesPart(location, Extension::extended));
    }
}

// --- Stations ----------------------------------------------------------------

TEST(Station, StraightCurve) {
    const auto curve = meaningOf(Segment{{1.0, 1.0}, {4.0, 5.0}});
    const auto at = [&curve](Point2 point) {
        return detail::station(curve, detail::exactPoint(point));
    };
    EXPECT_TRUE(sameBits(at({1.0, 1.0}), 0.0));
    EXPECT_TRUE(sameBits(at({4.0, 5.0}), 5.0));
    EXPECT_TRUE(sameBits(at({-2.0, -3.0}), -5.0));
    EXPECT_TRUE(sameBits(at({7.0, 9.0}), 10.0));
    // A point beyond the end of a tiny segment: no overflow.
    const auto tiny = meaningOf(Segment{{0.0, 0.0}, {0x1p-1074, 0.0}});
    EXPECT_TRUE(sameBits(detail::station(tiny, detail::exactPoint({0x1p49, 0.0})), 0x1p49));
}

// The angle of p from the centre c, from the direction d0, in the direction
// of the turn, in [0, 2 pi), in long double.
long double referenceAngle(Point2 c, Point2 p, long double d0x, long double d0y, bool clockwise) {
    const long double px = static_cast<long double>(p.x) - c.x;
    const long double py = static_cast<long double>(p.y) - c.y;
    long double angle = std::atan2(py, px) - std::atan2(d0y, d0x);
    if (clockwise) {
        angle = -angle;
    }
    const long double turn = 2.0L * std::numbers::pi_v<long double>;
    while (angle < 0.0L) {
        angle += turn;
    }
    while (angle >= turn) {
        angle -= turn;
    }
    return angle;
}

TEST(Station, CircleQuartersAndAccuracy) {
    const auto circle = meaningOf(Circle{{0.0, 0.0}, 2.0});
    const auto at = [&circle](Point2 point) {
        return detail::station(circle, detail::exactPoint(point));
    };
    EXPECT_TRUE(sameBits(at({2.0, 0.0}), 0.0));
    const double tolerance = kRelativeStationError * 4.0 * std::numbers::pi;
    EXPECT_NEAR(at({0.0, 2.0}), std::numbers::pi, tolerance);
    EXPECT_NEAR(at({-2.0, 0.0}), 2.0 * std::numbers::pi, tolerance);
    EXPECT_NEAR(at({0.0, -2.0}), 3.0 * std::numbers::pi, tolerance);
    // Points with radicals all around, against the long double reference.
    Sequence sequence(2);
    for (int i = 0; i < 500; ++i) {
        const Point2 through{sequence.between(1.5), sequence.between(1.5)};
        const Point2 towards{sequence.between(3.0), sequence.between(3.0)};
        if (through.x == towards.x && through.y == towards.y) {
            continue;
        }
        for (const auto& point : pointsOn(circle, through, towards)) {
            const auto p = detail::nearestPoint(point);
            const long double angle = referenceAngle({0.0, 0.0}, p, 1.0L, 0.0L, false);
            const auto s = detail::station(circle, point);
            // The rounded point moves the reference angle by up to 2^-52.
            EXPECT_NEAR(s, static_cast<double>(2.0L * angle),
                        kRelativeStationError * std::max(2.0, s) + 0x1p-50)
                << p.x << " " << p.y;
        }
    }
}

// Every station of the points of a curve, sorted exactly along it, never
// decreases; and equal points have equal bits.
void expectMonotone(const CurveMeaning& curve, std::vector<ExactPoint> points) {
    std::sort(points.begin(), points.end(), [&curve](const ExactPoint& a, const ExactPoint& b) {
        return detail::compareAlong(curve, a, b) == Sign::negative;
    });
    for (std::size_t i = 1; i < points.size(); ++i) {
        const auto order = detail::compareAlong(curve, points[i - 1], points[i]);
        ASSERT_NE(order, Sign::positive);
        const double before = detail::station(curve, points[i - 1]);
        const double after = detail::station(curve, points[i]);
        EXPECT_LE(before, after) << i;
        if (order == Sign::zero) {
            EXPECT_TRUE(sameBits(before, after));
        }
    }
}

// Lines through the centre almost along the eight half-axes and diagonals:
// their points straddle the bounds of the eighths of a turn by a few ulp.
std::vector<ExactPoint> nearBounds(const CurveMeaning& curve, Point2 centre) {
    std::vector<ExactPoint> points;
    for (const double x : {1.0, previous(1.0), next(1.0)}) {
        for (const double y : {0.0, 0x1p-1074, -0x1p-1074, 1.0, previous(1.0), next(1.0)}) {
            for (const Point2 direction : {Point2{x, y}, Point2{-y, x}}) {
                const Point2 far{centre.x + direction.x, centre.y + direction.y};
                for (auto& point : pointsOn(curve, centre, far)) {
                    points.push_back(std::move(point));
                }
            }
        }
    }
    return points;
}

TEST(Station, NeverDecreasesAlongCurves) {
    struct Sample {
        Curve curve;
        Point2 centre;  // the centre of a circular curve, unused for a line
    };

    const std::vector<Sample> samples{
        {Circle{{0.0, 0.0}, 1.0}, {0.0, 0.0}},
        {Circle{{0.25, -0.5}, 3.0}, {0.25, -0.5}},
        {Arc{{0.0, 0.0}, 1.0, 0.3, 2.0}, {0.0, 0.0}},
        {Arc{{0.5, 0.5}, 2.0, -1.0, -4.0}, {0.5, 0.5}},
        {Arc{{0.0, 0.0}, 1.0, 1.0, 7.0}, {0.0, 0.0}},
        // Centre (0, 0) for both: chords of the unit circle.
        {PolylineSegment{{1.0, 0.0}, {0.0, 1.0}, std::numbers::sqrt2 - 1.0}, {0.0, 0.0}},
        {PolylineSegment{{1.0, 0.0}, {-1.0, 0.0}, -1.0}, {0.0, 0.0}},
        {Segment{{0.0, 0.0}, {3.0, 1.0}}, {0.0, 0.0}},
        {PolylineSegment{{2.0, 2.0}, {-1.0, 0.5}, 0.0}, {0.0, 0.0}},
    };
    Sequence sequence(3);
    for (const auto& sample : samples) {
        const auto curve = meaningOf(sample.curve);
        auto points = std::holds_alternative<detail::LineMeaning>(curve.support)
                          ? std::vector<ExactPoint>{}
                          : nearBounds(curve, sample.centre);
        for (int i = 0; i < 150; ++i) {
            const Point2 a{sequence.between(3.0), sequence.between(3.0)};
            const Point2 b{sequence.between(3.0), sequence.between(3.0)};
            if (a.x == b.x && a.y == b.y) {
                continue;
            }
            for (auto& point : pointsOn(curve, a, b)) {
                points.push_back(std::move(point));
            }
        }
        expectMonotone(curve, std::move(points));
    }
}

TEST(Station, SameValueSameBits) {
    // The end of an arc in direction D, written with D and with 3 D.
    const auto arc = meaningOf(Arc{{0.5, -0.25}, 1.5, 0.7, 1.1});
    const auto& meaning = std::get<detail::ArcMeaning>(arc.support);
    const auto once = detail::pointInDirection(meaning.circle, meaning.endDirection);
    const ExactVector thrice{meaning.endDirection.x * Dyadic(3.0),
                             meaning.endDirection.y * Dyadic(3.0)};
    const auto other = detail::pointInDirection(meaning.circle, thrice);
    ASSERT_TRUE(detail::samePoint(once, other));
    const auto p = detail::nearestPoint(once);
    const auto q = detail::nearestPoint(other);
    EXPECT_TRUE(sameBits(p.x, q.x));
    EXPECT_TRUE(sameBits(p.y, q.y));
    EXPECT_TRUE(sameBits(detail::station(arc, once), detail::station(arc, other)));
    EXPECT_EQ(detail::locate(arc, once), Location::atEnd);
    EXPECT_EQ(detail::locate(arc, other), Location::atEnd);
    EXPECT_EQ(detail::compareAlong(arc, once, other), Sign::zero);
}

TEST(Station, ArcsMeasureFromTheirStart) {
    // A clockwise half circle from (1, 0) through (0, -1): the station of
    // (0, -1) is pi / 2, of (-1, 0) pi, of (0, 1) on the extension 3 pi / 2.
    const auto half = meaningOf(PolylineSegment{{1.0, 0.0}, {-1.0, 0.0}, -1.0});
    const auto at = [&half](Point2 point) {
        return detail::station(half, detail::exactPoint(point));
    };
    const double tolerance = kRelativeStationError * 2.0 * std::numbers::pi;
    EXPECT_TRUE(sameBits(at({1.0, 0.0}), 0.0));
    EXPECT_NEAR(at({0.0, -1.0}), std::numbers::pi / 2.0, tolerance);
    EXPECT_NEAR(at({-1.0, 0.0}), std::numbers::pi, tolerance);
    EXPECT_NEAR(at({0.0, 1.0}), 1.5 * std::numbers::pi, tolerance);
    // A huge circle (bulge 2^-30): the station near the start keeps its
    // relative accuracy, far below the radius.
    const auto flat = meaningOf(PolylineSegment{{0.0, 0.0}, {1.0, 0.0}, 0x1p-30});
    const auto end = detail::station(flat, detail::endPoint(flat));
    EXPECT_NEAR(end, 1.0, 0x1p-40);
}

}  // namespace
