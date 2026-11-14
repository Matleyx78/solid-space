// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "solid_space/geom2d/ellipse.hpp"
#include "solid_space/robust/trigonometry.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <utility>

namespace {

namespace geom2d = solid_space::geom2d;
namespace robust = solid_space::robust;
using geom2d::Ellipse;
using geom2d::Location;
using geom2d::Point2;

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

    // A double in [low, high] with 30 random bits.
    double between(double low, double high) {
        const auto value = static_cast<double>(bits() >> 34U);
        return low + (high - low) * (value / 0x1p30);
    }

private:
    std::uint64_t m_state;
};

// A point of the ellipse at parameter t, from the doubles of robust::sinCos:
// within a few ulp of M of the exact point, far below the error of nearest().
Point2 sample(const Ellipse& ellipse, double t) {
    const auto [sine, cosine] = robust::sinCos(t);
    const Point2 minor{-ellipse.ratio * ellipse.majorAxis.y, ellipse.ratio * ellipse.majorAxis.x};
    return {ellipse.center.x + cosine * ellipse.majorAxis.x + sine * minor.x,
            ellipse.center.y + cosine * ellipse.majorAxis.y + sine * minor.y};
}

double distance(Point2 a, Point2 b) {
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
}

double magnitudeOf(const Ellipse& ellipse, Point2 p) {
    return std::max({std::fabs(ellipse.center.x), std::fabs(ellipse.center.y),
                     std::fabs(ellipse.majorAxis.x), std::fabs(ellipse.majorAxis.y), std::fabs(p.x),
                     std::fabs(p.y)});
}

bool sameBits(Point2 a, Point2 b) {
    return std::bit_cast<std::uint64_t>(a.x) == std::bit_cast<std::uint64_t>(b.x) &&
           std::bit_cast<std::uint64_t>(a.y) == std::bit_cast<std::uint64_t>(b.y);
}

// True when the curve as given (never its extension) has a point within
// `error` of the point: meetsRectangle() decides exactly whether it meets the
// closed square of half side 0.7 error around it, which lies inside that
// disc.
bool onCurve(const Ellipse& ellipse, Point2 point, double error) {
    const double half = 0.7 * error;
    const auto meets = geom2d::meetsRectangle(ellipse, {point.x - half, point.y - half},
                                              {point.x + half, point.y + half});
    return meets.hasValue() && meets.value();
}

// The parameter interval of the arc as sampled: [start, start + sweep].
double sweepOf(const Ellipse& ellipse) {
    double sweep = ellipse.endParameter - ellipse.startParameter;
    while (sweep <= 0.0) {
        sweep += geom2d::kFullTurn;
    }
    while (sweep > geom2d::kFullTurn) {
        sweep -= geom2d::kFullTurn;
    }
    return sweep;
}

// The properties that hold for every lawful choice: the point is on the curve
// as given within the error; no sampled point of the curve is closer than
// the distance less the error; the distance agrees with the point; an end has
// the bits of endpoints(); a whole ellipse is inside.
void checkNearest(const Ellipse& ellipse, Point2 p) {
    const auto result = geom2d::nearest(ellipse, p);
    ASSERT_TRUE(result.hasValue());
    const auto& nearest = result.value();
    const double magnitude = magnitudeOf(ellipse, p);
    const double error = geom2d::kRelativeNearestError * magnitude;
    // The rounding of the samples and of the distances computed here: a few
    // ulp of numbers at most 6 N.
    const double rounding = 0x1p-48 * magnitude;
    // d <= |p - q| + error <= d* + 2 error, and d* is at most the distance of
    // any sample.
    const double slack = 2.0 * error + rounding;
    EXPECT_GE(nearest.distance, 0.0);
    EXPECT_NEAR(nearest.distance, distance(p, nearest.point), error + rounding);
    EXPECT_TRUE(onCurve(ellipse, nearest.point, error)) << "p = (" << p.x << ", " << p.y << ")";

    constexpr int kSamples = 2000;
    const double sweep = sweepOf(ellipse);
    for (int i = 0; i <= kSamples; ++i) {
        const double t = ellipse.startParameter + sweep * i / kSamples;
        EXPECT_LE(nearest.distance, distance(p, sample(ellipse, t)) + slack)
            << "t = " << t << ", p = (" << p.x << ", " << p.y << ")";
    }

    const auto ends = geom2d::endpoints(ellipse);
    ASSERT_TRUE(ends.hasValue());
    const bool whole = sweep == geom2d::kFullTurn;
    switch (nearest.location) {
        case Location::atStart:
            EXPECT_FALSE(whole);
            EXPECT_TRUE(sameBits(nearest.point, ends.value().start));
            break;
        case Location::atEnd:
            EXPECT_FALSE(whole);
            EXPECT_TRUE(sameBits(nearest.point, ends.value().end));
            break;
        case Location::inside:
            break;
        default:
            ADD_FAILURE() << "unexpected location";
    }
}

TEST(EllipseNearest, NoSampleIsCloserOnGeneratedData) {
    Sequence random(20261114);
    // Millimetres and kilometres far from the origin (ADR 0009): the centre
    // moves, so that the exact coefficients have long mantissas.
    constexpr std::array<std::pair<double, double>, 2> kScales{{{1.0, 0.0}, {1e3, 123456.789}}};
    for (const auto& entry : kScales) {
        const double scale = entry.first;
        const double offset = entry.second;
        const auto coordinate = [&random, scale] {
            return scale * random.between(-10.0, 10.0);
        };
        const auto ratio = [&random] {
            return random.between(0.01, 1.0);
        };
        const auto parameter = [&random] {
            return random.between(-7.0, 7.0);
        };
        for (int i = 0; i < 60; ++i) {
            const Ellipse ellipse{{offset + coordinate(), coordinate() - offset},
                                  {coordinate(), coordinate()},
                                  ratio(),
                                  parameter(),
                                  parameter()};
            const Point2 near{ellipse.center.x, ellipse.center.y};
            for (int j = 0; j < 5; ++j) {
                checkNearest(ellipse, {near.x + coordinate(), near.y + coordinate()});
            }
            const Ellipse whole{ellipse.center, ellipse.majorAxis, ellipse.ratio, 0.0,
                                geom2d::kFullTurn};
            checkNearest(whole, {near.x + coordinate(), near.y + coordinate()});
        }
    }
}

TEST(EllipseNearest, WholeEllipseIsNeverAtAnEnd) {
    for (const double start : {0.0, 0.2, -3.0}) {
        const Ellipse ellipse{{1.0, 2.0}, {4.0, 3.0}, 0.5, start, start};
        // The start itself and the point beyond it along the normal.
        const auto ends = geom2d::endpoints(ellipse);
        ASSERT_TRUE(ends.hasValue());
        const Point2 at = ends.value().start;
        const Point2 beyond{2.0 * at.x - 1.0, 2.0 * at.y - 2.0};
        for (const Point2 p : {at, beyond, Point2{1.0, 2.0}}) {
            const auto result = geom2d::nearest(ellipse, p);
            ASSERT_TRUE(result.hasValue());
            EXPECT_EQ(result.value().location, Location::inside);
            checkNearest(ellipse, p);
        }
    }
}

TEST(EllipseNearest, EndHasTheBitsOfEndpoints) {
    // Beyond the end of the arc t in [-0.8, 0] the end is nearest (#217,
    // case 3b), at two scales.
    for (const double scale : {1.0, 1e5}) {
        const Ellipse ellipse{{0.0, 0.0}, {10.0 * scale, 0.0}, 0.5, -0.8, 0.0};
        const auto ends = geom2d::endpoints(ellipse);
        ASSERT_TRUE(ends.hasValue());
        const auto atEnd = geom2d::nearest(ellipse, {12.0 * scale, 2.0 * scale});
        ASSERT_TRUE(atEnd.hasValue());
        EXPECT_EQ(atEnd.value().location, Location::atEnd);
        EXPECT_TRUE(sameBits(atEnd.value().point, ends.value().end));
        const auto atStart = geom2d::nearest(ellipse, {0.0, -10.0 * scale});
        ASSERT_TRUE(atStart.hasValue());
        EXPECT_EQ(atStart.value().location, Location::atStart);
        EXPECT_TRUE(sameBits(atStart.value().point, ends.value().start));
    }
}

TEST(EllipseNearest, NearTheEvoluteForSeveralRatios) {
    // On the major axis the evolute has its cusp at x = (a^2 - b^2) / a; the
    // feet off the axis merge there. Points just before, at and after it, on
    // the axis and slightly off it, and near the other cusp on the minor
    // axis, y = (b^2 - a^2) / b.
    for (const double ratio : {1.0, 0.5, 0.01, 1e-6}) {
        for (const double scale : {1.0, 1e6}) {
            const double a = 10.0 * scale;
            const double b = ratio * a;
            const Ellipse ellipse{{0.0, 0.0}, {a, 0.0}, ratio, 0.0, geom2d::kFullTurn};
            const double cusp = (a * a - b * b) / a;
            const double minorCusp = ratio == 1.0 ? 0.0 : (b * b - a * a) / b;
            for (const double shift : {-1e-3, -1e-9, 0.0, 1e-9, 1e-3}) {
                for (const double off : {0.0, 1e-12, 1e-6}) {
                    checkNearest(ellipse, {cusp + shift * a, off * a});
                    checkNearest(ellipse, {-cusp - shift * a, -off * a});
                    if (std::fabs(minorCusp) <= robust::kMaxCoordinate) {
                        checkNearest(ellipse, {off * a, minorCusp + shift * a});
                    }
                }
            }
        }
    }
}

TEST(EllipseNearest, NearTheEvoluteOfATurnedEllipse) {
    // The same cusps on an ellipse whose axis is not along x, and an arc that
    // ends close to the feet.
    for (const double ratio : {0.5, 0.01, 1e-6}) {
        const Ellipse ellipse{{3.0, -2.0}, {6.0, 8.0}, ratio, 0.0, geom2d::kFullTurn};
        const double a = 10.0;
        const double b = ratio * a;
        const double cusp = (a * a - b * b) / a;
        for (const double shift : {-1e-6, 0.0, 1e-6}) {
            const double along = cusp + shift;
            checkNearest(ellipse, {3.0 + along * 0.6, -2.0 + along * 0.8});
            const Ellipse arc{ellipse.center, ellipse.majorAxis, ratio, -0.01, 0.3};
            checkNearest(arc, {3.0 + along * 0.6, -2.0 + along * 0.8});
        }
    }
}

TEST(EllipseNearest, CircleSeenFromItsCentre) {
    const Ellipse circle{{1.0, 1.0}, {3.0, 4.0}, 1.0, 0.5, 0.5};
    const auto whole = geom2d::nearest(circle, {1.0, 1.0});
    ASSERT_TRUE(whole.hasValue());
    EXPECT_EQ(whole.value().location, Location::inside);
    EXPECT_NEAR(whole.value().distance, 5.0, 1e-12);
    const Ellipse arc{{1.0, 1.0}, {3.0, 4.0}, 1.0, 0.5, 2.0};
    const auto onArc = geom2d::nearest(arc, {1.0, 1.0});
    ASSERT_TRUE(onArc.hasValue());
    EXPECT_NEAR(onArc.value().distance, 5.0, 1e-12);
    checkNearest(arc, {1.0, 1.0});
}

TEST(EllipseNearest, PointOnTheCurveAndOnTheOppositeVertex) {
    // A point on the curve gives a distance within the error of zero; the
    // vertex at parameter pi is a foot reached only through Q == 0.
    const Ellipse ellipse{{0.0, 0.0}, {10.0, 0.0}, 0.5, 0.0, geom2d::kFullTurn};
    for (const Point2 p :
         {Point2{-10.0, 0.0}, Point2{-12.0, 0.0}, Point2{0.0, 5.0}, Point2{6.0, 4.0}}) {
        checkNearest(ellipse, p);
    }
    const auto vertex = geom2d::nearest(ellipse, {-12.0, 0.0});
    ASSERT_TRUE(vertex.hasValue());
    EXPECT_NEAR(vertex.value().point.x, -10.0, 1e-12);
    EXPECT_NEAR(vertex.value().distance, 2.0, 1e-12);
}

TEST(EllipseNearest, DiagnosesInTheOrderOfTheHeader) {
    const double nan = std::nan("");
    const Point2 inside{0.0, 0.0};
    const std::array<std::pair<Ellipse, Point2>, 5> cases{{
        {Ellipse{{0.0, 0.0}, {10.0, 0.0}, 0.0}, {nan, 0.0}},
        {Ellipse{{0.0, 0.0}, {0x1p51, 0.0}, 0.5}, {nan, 0.0}},
        {Ellipse{{0.0, 0.0}, {0.0, 0.0}, 0.5}, {nan, 0.0}},
        {Ellipse{{0.0, 0.0}, {10.0, 0.0}, 0.5}, {nan, 0.0}},
        {Ellipse{{0.0, 0.0}, {10.0, 0.0}, 0.5}, {0x1p51, 0.0}},
    }};
    const std::array<std::pair<geom2d::DiagnosisCode, std::size_t>, 5> expected{{
        {geom2d::DiagnosisCode::invalidValue, 0},
        {geom2d::DiagnosisCode::outOfDomain, 0},
        {geom2d::DiagnosisCode::degenerateCurve, 0},
        {geom2d::DiagnosisCode::invalidValue, 1},
        {geom2d::DiagnosisCode::outOfDomain, 1},
    }};
    for (std::size_t i = 0; i < cases.size(); ++i) {
        const auto& [ellipse, p] = cases.at(i);
        const auto& [code, input] = expected.at(i);
        const auto result = geom2d::nearest(ellipse, p);
        ASSERT_FALSE(result.hasValue());
        EXPECT_EQ(result.diagnosis().code, code);
        EXPECT_EQ(result.diagnosis().input, input);
    }
    EXPECT_TRUE(geom2d::nearest(Ellipse{{0.0, 0.0}, {10.0, 0.0}, 0.5}, inside).hasValue());
}

}  // namespace
