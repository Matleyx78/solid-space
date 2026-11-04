// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// offsetPolyline (offset.hpp): the properties of the «Verifica» of ADR 0012 on
// data generated deterministically, and the behaviours of the steps that the
// cases of tests/corpus/ do not reach. The geometric truth of the requested
// cases is in tests/corpus/ (ADR 0004).

#include "curve_meaning.hpp"
#include "exact/dyadic.hpp"
#include "exact/quadratic.hpp"
#include "solid_space/geom2d/intersection.hpp"
#include "solid_space/geom2d/offset.hpp"
#include "solid_space/robust/tolerance.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <numbers>
#include <string>
#include <variant>
#include <vector>

namespace {

namespace g = solid_space::geom2d;
namespace r = solid_space::robust;
namespace detail = solid_space::geom2d::detail;
namespace exact = solid_space::geom2d::exact;

using exact::Dyadic;

r::TolerancePolicy millimetres() {
    return r::TolerancePolicy::forUnit(0.001).value();
}

g::Polyline polyline(std::initializer_list<g::PolylineVertex> vertices, bool closed = false) {
    return g::Polyline{std::vector<g::PolylineVertex>(vertices), closed};
}

g::PolylineOffset offset(const g::Polyline& input, double distance) {
    const auto result = g::offsetPolyline(input, distance, millimetres());
    EXPECT_TRUE(result.hasValue()) << (result.hasValue() ? "" : result.diagnosis().message);
    return result.hasValue() ? result.value() : g::PolylineOffset{};
}

// A linear congruential generator (Knuth's MMIX constants): the same numbers
// on every platform, unlike the distributions of <random>.
class Generator {
public:
    explicit Generator(std::uint64_t seed) : m_state(seed) {}

    // An integer in [low, high].
    int between(int low, int high) {
        m_state = m_state * 6364136223846793005ULL + 1442695040888963407ULL;
        const auto span = static_cast<std::uint64_t>(high) - static_cast<std::uint64_t>(low) + 1U;
        return low + static_cast<int>((m_state >> 33U) % span);
    }

    template <typename T, std::size_t N>
    const T& pick(const std::array<T, N>& values) {
        return values.at(static_cast<std::size_t>(between(0, static_cast<int>(N) - 1)));
    }

private:
    std::uint64_t m_state;
};

// The magnitude M of an offset (offset.hpp): of all the pieces and |d|.
double magnitudeOf(const g::Polyline& input, double distance) {
    double result = std::abs(distance);
    for (const auto& piece : g::pieces(input)) {
        const auto value = g::magnitude(piece);
        if (value.hasValue()) {
            result = std::max(result, value.value());
        }
    }
    return result;
}

double pointError(double magnitude, g::Point2 point) {
    return g::kRelativePointError * std::max({magnitude, std::abs(point.x), std::abs(point.y)});
}

bool nearShared(g::Point2 point, const std::vector<g::Point2>& shared,
                const r::TolerancePolicy& policy) {
    return std::ranges::any_of(shared,
                               [&](g::Point2 vertex) { return policy.coincident(point, vertex); });
}

// The check of offset.hpp redone with intersect(): pieces not consecutive have
// no common point, consecutive ones meet only near their shared vertices, and
// no piece meets the input.
testing::AssertionResult passesCheck(const g::Polyline& input, const g::PolylineOffset& result,
                                     const r::TolerancePolicy& policy) {
    const auto own = g::pieces(result.polyline);
    const auto& vertices = result.polyline.vertices;
    const auto count = own.size();
    const auto degenerate = [](const g::Curve& curve) {
        return detail::check(curve, 0).has_value();
    };
    for (std::size_t i = 0; i < count; ++i) {
        for (std::size_t j = i + 1; j < count; ++j) {
            if (degenerate(own[i]) || degenerate(own[j])) {
                continue;
            }
            std::vector<g::Point2> shared;
            if (j == i + 1) {
                shared.push_back(vertices[j].position);
            }
            if (result.polyline.closed && i == 0 && j + 1 == count) {
                shared.push_back(vertices[0].position);
            }
            const auto meeting =
                g::intersect(own[i], g::Extension::none, own[j], g::Extension::none);
            if (!meeting.hasValue()) {
                return testing::AssertionFailure() << "pieces " << i << ", " << j << ": diagnosis";
            }
            for (const auto& point : meeting.value().points) {
                if (!nearShared(point.point, shared, policy)) {
                    return testing::AssertionFailure()
                           << "pieces " << i << " and " << j << " meet at (" << point.point.x
                           << ", " << point.point.y << ")";
                }
            }
            for (const auto& overlap : meeting.value().overlaps) {
                const bool small = std::ranges::any_of(shared, [&](g::Point2 vertex) {
                    return policy.coincident(overlap.from.point, vertex) &&
                           policy.coincident(overlap.to.point, vertex);
                });
                if (overlap.whole || !small) {
                    return testing::AssertionFailure()
                           << "pieces " << i << " and " << j << " overlap";
                }
            }
        }
    }
    for (std::size_t i = 0; i < count; ++i) {
        for (const auto& piece : g::pieces(input)) {
            if (degenerate(own[i]) || degenerate(piece)) {
                continue;
            }
            const auto meeting =
                g::intersect(own[i], g::Extension::none, piece, g::Extension::none);
            if (!meeting.hasValue() || !meeting.value().points.empty() ||
                !meeting.value().overlaps.empty()) {
                return testing::AssertionFailure() << "piece " << i << " meets the input";
            }
        }
    }
    return testing::AssertionSuccess();
}

// True when the point lies within `error` of the carrier of the piece moved by
// d, decided exactly:
// - a line from P with direction U, moved to cross(U, X - P) = d sqrt(L) with
//   L = |U|^2: |cross(U, X - P) / sqrt(L) - d| <= error;
// - a circle C / w with squared radius rho / w^2, moved to the radius
//   (sqrt(rho) - s d w) / w (s = 1 counterclockwise, -1 clockwise):
//   |sqrt(S) - sqrt(rho) + s d w| <= error w with S = |X w - C|^2.
bool nearOffsetCarrier(const g::PolylineSegment& piece, double distance, g::Point2 point,
                       double error) {
    const auto meaning = detail::meaningOf(piece);
    const Dyadic d(distance);
    const Dyadic e(error);
    const Dyadic x(point.x);
    const Dyadic y(point.y);
    if (std::holds_alternative<detail::LineMeaning>(meaning.support)) {
        const Dyadic ux = Dyadic(piece.end.x) - Dyadic(piece.start.x);
        const Dyadic uy = Dyadic(piece.end.y) - Dyadic(piece.start.y);
        const Dyadic squaredLength = ux * ux + uy * uy;
        const Dyadic cross = ux * (y - Dyadic(piece.start.y)) - uy * (x - Dyadic(piece.start.x));
        return exact::signOf(cross, -(d + e), squaredLength) != r::Sign::positive &&
               exact::signOf(cross, -(d - e), squaredLength) != r::Sign::negative;
    }
    const auto& arc = std::get<detail::ArcMeaning>(meaning.support);
    const auto& circle = arc.circle;
    const Dyadic s = arc.turn == detail::Turn::counterclockwise ? d : -d;
    const Dyadic dx = x * circle.weight - circle.centerX;
    const Dyadic dy = y * circle.weight - circle.centerY;
    const Dyadic squared = dx * dx + dy * dy;
    const auto signWith = [&](const Dyadic& shift) {
        // sqrt(S) - sqrt(rho) + shift.
        return exact::signOf(exact::QuadraticNumber{shift, Dyadic(-1.0), circle.radiusSquared},
                             exact::QuadraticNumber{Dyadic(1.0), Dyadic(), Dyadic()}, squared);
    };
    return signWith((s - e) * circle.weight) != r::Sign::positive &&
           signWith((s + e) * circle.weight) != r::Sign::negative;
}

// Every vertex between two source pieces of the result (a joint that is not
// an arc, or ends that needed none) lies at |d| from the carriers of both
// source pieces, within the declared error.
testing::AssertionResult jointsAtDistance(const g::Polyline& input, double distance,
                                          const g::PolylineOffset& result) {
    const auto source = g::pieces(input);
    const auto& origins = result.origins;
    const auto& vertices = result.polyline.vertices;
    const auto count = origins.size();
    const double magnitude = magnitudeOf(input, distance);
    for (std::size_t k = 0; k < count; ++k) {
        if (k == 0 && !result.polyline.closed) {
            continue;
        }
        const auto& before = origins[(k + count - 1) % count];
        const auto& after = origins[k];
        if (before.origin != g::PieceOrigin::source || after.origin != g::PieceOrigin::source) {
            continue;
        }
        const auto point = vertices[k].position;
        const double error = pointError(magnitude, point);
        for (const auto index : {before.index, after.index}) {
            if (!nearOffsetCarrier(source[index], distance, point, error)) {
                return testing::AssertionFailure()
                       << "vertex " << k << " (" << point.x << ", " << point.y
                       << ") is not at the distance from piece " << index;
            }
        }
    }
    return testing::AssertionSuccess();
}

// A random polyline of segments and arcs with small integer vertices.
g::Polyline randomPolyline(Generator& generator) {
    static constexpr std::array<double, 10> kBulges{0.0,  0.0, 0.0,   0.0, 0.5,
                                                    -0.5, 1.0, -0.25, 2.0, -1.0};
    g::Polyline result;
    result.closed = generator.between(0, 2) == 0;
    const int count = generator.between(2, 7);
    for (int i = 0; i < count; ++i) {
        result.vertices.push_back({{static_cast<double>(generator.between(-12, 12)),
                                    static_cast<double>(generator.between(-12, 12))},
                                   generator.pick(kBulges)});
    }
    return result;
}

// A convex polygon, counterclockwise: the hull of random lattice points.
g::Polyline randomConvex(Generator& generator) {
    constexpr int kPoints = 12;
    std::vector<g::Point2> points;
    points.reserve(kPoints);
    for (int i = 0; i < kPoints; ++i) {
        points.push_back({static_cast<double>(generator.between(-30, 30)),
                          static_cast<double>(generator.between(-30, 30))});
    }
    std::ranges::sort(
        points, [](g::Point2 a, g::Point2 b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
    const auto turn = [](g::Point2 o, g::Point2 a, g::Point2 b) {
        return ((a.x - o.x) * (b.y - o.y)) - ((a.y - o.y) * (b.x - o.x));
    };
    std::vector<g::Point2> hull;
    for (int pass = 0; pass < 2; ++pass) {
        const auto start = hull.size();
        for (const auto point : points) {
            while (hull.size() >= start + 2 &&
                   turn(hull[hull.size() - 2], hull.back(), point) <= 0) {
                hull.pop_back();
            }
            hull.push_back(point);
        }
        hull.pop_back();
        std::ranges::reverse(points);
    }
    g::Polyline result;
    result.closed = true;
    for (const auto point : hull) {
        result.vertices.push_back({point, 0.0});
    }
    return result;
}

TEST(OffsetPolyline, EveryDoneOfRandomPolylinesPassesTheCheckAndKeepsItsJointsAtTheDistance) {
    static constexpr std::array<double, 8> kDistances{0.5, 1.0, -1.0, 2.0, -3.0, 4.5, -0.25, 7.0};
    Generator generator(20261101);
    const auto policy = millimetres();
    int done = 0;
    int notSimple = 0;
    for (int round = 0; round < 400; ++round) {
        const auto input = randomPolyline(generator);
        const double distance = generator.pick(kDistances);
        const auto result = g::offsetPolyline(input, distance, policy);
        if (!result.hasValue() || result.value().outcome != g::OffsetOutcome::done) {
            continue;
        }
        ++done;
        notSimple += result.value().simple ? 0 : 1;
        SCOPED_TRACE("round " + std::to_string(round));
        EXPECT_TRUE(passesCheck(input, result.value(), policy));
        EXPECT_TRUE(jointsAtDistance(input, distance, result.value()));
        EXPECT_EQ(result.value().origins.size(), g::pieces(result.value().polyline).size());
    }
    // The property is not vacuous: many results, some of them not simple.
    EXPECT_GE(done, 100);
    EXPECT_GE(notSimple, 10);
}

TEST(OffsetPolyline, ConvexPolygonsOffsetOutwardsAreSimple) {
    Generator generator(42);
    const auto policy = millimetres();
    for (int round = 0; round < 60; ++round) {
        const auto input = randomConvex(generator);
        if (input.vertices.size() < 3) {
            continue;
        }
        // Counterclockwise: right is outside.
        const double distance = -static_cast<double>(generator.between(1, 40)) / 4.0;
        SCOPED_TRACE("round " + std::to_string(round));
        const auto result = offset(input, distance);
        ASSERT_EQ(result.outcome, g::OffsetOutcome::done);
        EXPECT_TRUE(result.simple);
        EXPECT_EQ(result.polyline.vertices.size(), input.vertices.size());
        EXPECT_TRUE(passesCheck(input, result, policy));
        EXPECT_TRUE(jointsAtDistance(input, distance, result));
    }
}

TEST(OffsetPolyline, OnePieceAgreesWithOffsetCurve) {
    static constexpr std::array<double, 7> kBulges{0.0, 0.5, -0.5, 1.0, -1.0, 3.0, -0.125};
    static constexpr std::array<double, 6> kDistances{0.5, -0.5, 1.0, -2.0, 3.0, 1e-3};
    Generator generator(7);
    int done = 0;
    for (int round = 0; round < 300; ++round) {
        const g::PolylineSegment piece{
            {generator.between(-50, 50) / 8.0, generator.between(-50, 50) / 8.0},
            {generator.between(-50, 50) / 8.0, generator.between(-50, 50) / 8.0},
            generator.pick(kBulges)};
        const double distance = generator.pick(kDistances);
        const auto curve = g::offsetCurve(piece, distance);
        if (!curve.hasValue()) {
            continue;  // a single point
        }
        SCOPED_TRACE("round " + std::to_string(round));
        const auto result =
            offset(polyline({{piece.start, piece.bulge}, {piece.end, 0.0}}), distance);
        if (curve.value().outcome == g::OffsetOutcome::vanished) {
            EXPECT_EQ(result.outcome, g::OffsetOutcome::vanished);
            continue;
        }
        ++done;
        ASSERT_EQ(result.outcome, g::OffsetOutcome::done);
        EXPECT_TRUE(result.simple);
        ASSERT_EQ(result.polyline.vertices.size(), 2U);
        const auto& expected = std::get<g::PolylineSegment>(curve.value().curve);
        const auto& vertices = result.polyline.vertices;
        EXPECT_EQ(std::bit_cast<std::uint64_t>(vertices[0].position.x),
                  std::bit_cast<std::uint64_t>(expected.start.x));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(vertices[0].position.y),
                  std::bit_cast<std::uint64_t>(expected.start.y));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(vertices[1].position.x),
                  std::bit_cast<std::uint64_t>(expected.end.x));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(vertices[1].position.y),
                  std::bit_cast<std::uint64_t>(expected.end.y));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(vertices[0].bulge),
                  std::bit_cast<std::uint64_t>(expected.bulge));
        ASSERT_EQ(result.origins.size(), 1U);
        EXPECT_EQ(result.origins[0].origin, g::PieceOrigin::source);
    }
    EXPECT_GE(done, 150);
}

// Simple cases without joints: chains of pieces tangent at their vertices,
// whose moved ends meet exactly.
std::vector<g::Polyline> tangentChains(Generator& generator) {
    std::vector<g::Polyline> result;
    for (int round = 0; round < 20; ++round) {
        const double x = generator.between(-1000, 1000) / 4.0;
        const double y = generator.between(-1000, 1000) / 4.0;
        const double length = generator.between(1, 200) / 4.0;
        const double radius = generator.between(8, 40) / 2.0;
        // A slot: two segments and two half circles.
        result.push_back(polyline({{{x, y - radius}, 0.0},
                                   {{x + length, y - radius}, 1.0},
                                   {{x + length, y + radius}, 0.0},
                                   {{x, y + radius}, 1.0}},
                                  true));
        // A wave of half circles, alternately up and down.
        result.push_back(polyline({{{x, y}, 1.0},
                                   {{x + (2 * radius), y}, -1.0},
                                   {{x + (4 * radius), y}, 1.0},
                                   {{x + (6 * radius), y}, 0.0}}));
        // Collinear slanted segments: their moved ends meet on a root.
        result.push_back(polyline({{{x, y}, 0.0},
                                   {{x + length, y + (2 * length)}, 0.0},
                                   {{x + (3 * length), y + (6 * length)}, 0.0}}));
    }
    return result;
}

TEST(OffsetPolyline, OffsetAndBackReturnsTheInputWithinTwiceTheError) {
    Generator generator(99);
    const auto policy = millimetres();
    for (const auto& input : tangentChains(generator)) {
        const double distance =
            generator.between(1, 7) / 2.0 * (generator.between(0, 1) == 0 ? 1.0 : -1.0);
        const auto there = g::offsetPolyline(input, distance, policy);
        ASSERT_TRUE(there.hasValue());
        ASSERT_EQ(there.value().outcome, g::OffsetOutcome::done);
        ASSERT_TRUE(there.value().simple);
        const auto back = g::offsetPolyline(there.value().polyline, -distance, policy);
        ASSERT_TRUE(back.hasValue());
        ASSERT_EQ(back.value().outcome, g::OffsetOutcome::done);
        ASSERT_TRUE(back.value().simple);
        const auto& vertices = back.value().polyline.vertices;
        ASSERT_EQ(vertices.size(), input.vertices.size());
        const double magnitude = magnitudeOf(input, distance);
        for (std::size_t k = 0; k < vertices.size(); ++k) {
            const auto expected = input.vertices[k].position;
            const double error = 2.0 * pointError(magnitude, expected);
            EXPECT_LE(std::abs(vertices[k].position.x - expected.x), error) << "vertex " << k;
            EXPECT_LE(std::abs(vertices[k].position.y - expected.y), error) << "vertex " << k;
            EXPECT_EQ(vertices[k].bulge, input.vertices[k].bulge) << "vertex " << k;
        }
    }
}

TEST(OffsetPolyline, FreeEndKeepsItsCut) {
    // ADR 0012, table, case 3: the short side turns backwards and goes; the
    // long side keeps the start that the joint gave it.
    const auto result = offset(polyline({{{0, 0}, 0}, {{4, 0}, 0}, {{4, 20}, 0}}), 5.0);
    ASSERT_EQ(result.outcome, g::OffsetOutcome::done);
    EXPECT_FALSE(result.simple);
    ASSERT_EQ(result.polyline.vertices.size(), 2U);
    EXPECT_EQ(result.polyline.vertices[0].position.x, -1.0);
    EXPECT_EQ(result.polyline.vertices[0].position.y, 5.0);
    EXPECT_EQ(result.polyline.vertices[1].position.x, -1.0);
    EXPECT_EQ(result.polyline.vertices[1].position.y, 20.0);
    ASSERT_EQ(result.origins.size(), 1U);
    EXPECT_EQ(result.origins[0].index, 1U);
}

TEST(OffsetPolyline, ArcJointAfterRepeatedVerticesTurnsAroundTheEndOfA) {
    // The pieces 0 and 2 are made consecutive by the point piece 1: the arc
    // turns around vertex 1, at the end of the source piece of A.
    const auto result =
        offset(polyline({{{0, 0}, 0}, {{10, 0}, 0}, {{10, 0}, 0}, {{0, 0}, 0}}), -1.0);
    ASSERT_EQ(result.outcome, g::OffsetOutcome::done);
    EXPECT_TRUE(result.simple);
    ASSERT_EQ(result.origins.size(), 3U);
    EXPECT_EQ(result.origins[0].origin, g::PieceOrigin::source);
    EXPECT_EQ(result.origins[0].index, 0U);
    EXPECT_EQ(result.origins[1].origin, g::PieceOrigin::gap);
    EXPECT_EQ(result.origins[1].index, 1U);
    EXPECT_EQ(result.origins[2].origin, g::PieceOrigin::source);
    EXPECT_EQ(result.origins[2].index, 2U);
    // A negative distance: the counterclockwise half circle.
    EXPECT_EQ(result.polyline.vertices[1].bulge, 1.0);
}

TEST(OffsetPolyline, ArcJointTurnsAsTheInputTurns) {
    // A right angle turning left, offset outwards (to the right): the arc of
    // radius 2 around (10, 0), a quarter turn counterclockwise.
    const auto result = offset(polyline({{{0, 0}, 0}, {{10, 0}, 0}, {{10, 10}, 0}}), -2.0);
    ASSERT_EQ(result.outcome, g::OffsetOutcome::done);
    // Two straight lines always meet: no arc here; the corner is sharp.
    ASSERT_EQ(result.origins.size(), 2U);
    EXPECT_EQ(result.polyline.vertices[1].position.x, 12.0);
    EXPECT_EQ(result.polyline.vertices[1].position.y, -2.0);
    // A segment, then a clockwise half circle of radius 2 around (12, 0) that
    // leaves (10, 0) upwards: a left turn. Moved by 1.5 to the right, the line
    // y = -1.5 and the circle of radius 0.5 do not meet: the gap arc of radius
    // 1.5 around (10, 0), from (10, -1.5) to (11.5, 0), a quarter turn
    // counterclockwise, bulge tan(pi / 8).
    const auto curved = offset(polyline({{{0, 0}, 0}, {{10, 0}, -1}, {{14, 0}, 0}}), -1.5);
    ASSERT_EQ(curved.outcome, g::OffsetOutcome::done);
    EXPECT_TRUE(curved.simple);
    ASSERT_EQ(curved.origins.size(), 3U);
    EXPECT_EQ(curved.origins[1].origin, g::PieceOrigin::gap);
    EXPECT_EQ(curved.origins[1].index, 1U);
    EXPECT_NEAR(curved.polyline.vertices[1].bulge, 0.41421356237309503, 1e-15);
    EXPECT_EQ(curved.polyline.vertices[1].position.x, 10.0);
    EXPECT_EQ(curved.polyline.vertices[1].position.y, -1.5);
    EXPECT_EQ(curved.polyline.vertices[2].position.x, 11.5);
    EXPECT_EQ(curved.polyline.vertices[2].position.y, 0.0);
}

TEST(OffsetPolyline, ClosedPolylineWithOnePieceLeftFails) {
    // A half disc: the half circle vanishes, the diameter is left alone.
    const auto result = offset(polyline({{{-1, 0}, 1}, {{1, 0}, 0}}, true), 1.5);
    EXPECT_EQ(result.outcome, g::OffsetOutcome::failed);
    EXPECT_EQ(result.failure, g::OffsetFailure::openJoint);
    EXPECT_FALSE(result.simple);
}

TEST(OffsetPolyline, EveryPieceVanishing) {
    const auto circle = polyline({{{-1, 0}, 1}, {{1, 0}, 1}}, true);
    for (const double distance : {1.0, 2.0}) {
        const auto result = offset(circle, distance);
        EXPECT_EQ(result.outcome, g::OffsetOutcome::vanished);
        EXPECT_EQ(result.failure, g::OffsetFailure::none);
        EXPECT_FALSE(result.simple);
    }
    const auto outside = offset(circle, -1.0);
    ASSERT_EQ(outside.outcome, g::OffsetOutcome::done);
    EXPECT_TRUE(outside.simple);
}

TEST(OffsetPolyline, TheDiagnosisNamesTheVertex) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const auto result =
        g::offsetPolyline(polyline({{{0, 0}, 0}, {{1, 0}, 0}, {{2, nan}, 0}}), 1.0, millimetres());
    ASSERT_FALSE(result.hasValue());
    EXPECT_EQ(result.diagnosis().code, g::DiagnosisCode::invalidValue);
    EXPECT_EQ(result.diagnosis().input, 0U);
    EXPECT_NE(result.diagnosis().message.find("vertex 2"), std::string::npos)
        << result.diagnosis().message;
    // A single vertex has no piece: degenerate.
    const auto alone = g::offsetPolyline(polyline({{{0, 0}, 0}}), 1.0, millimetres());
    ASSERT_FALSE(alone.hasValue());
    EXPECT_EQ(alone.diagnosis().code, g::DiagnosisCode::degenerateCurve);
}

TEST(OffsetPolyline, SlantedPiecesShareTheirRoots) {
    // Joints of slanted segments and arcs: the moved ends and the meetings
    // carry several roots, each decision in its own tower.
    const auto input = polyline(
        {{{0, 0}, 0}, {{3, 1}, 0.3}, {{7, 2}, 0}, {{9, 7}, -0.2}, {{4, 9}, 0}, {{1, 5}, 0}}, true);
    const auto policy = millimetres();
    for (const double distance : {0.25, -0.25, 0.75, -1.5}) {
        SCOPED_TRACE(distance);
        const auto result = offset(input, distance);
        ASSERT_EQ(result.outcome, g::OffsetOutcome::done);
        EXPECT_TRUE(passesCheck(input, result, policy));
        EXPECT_TRUE(jointsAtDistance(input, distance, result));
    }
}

// Package P19: rounding, bulges, collapsed pieces and the exact domain.

constexpr double kLimit = r::kMaxCoordinate;
constexpr double kPi = std::numbers::pi;

void expectFailure(const g::PolylineOffset& result,
                   std::initializer_list<g::OffsetFailure> allowed) {
    EXPECT_EQ(result.outcome, g::OffsetOutcome::failed);
    EXPECT_TRUE(std::ranges::find(allowed, result.failure) != allowed.end())
        << static_cast<int>(result.failure);
}

TEST(OffsetPolyline, OnlyTheExactEndsOfThePiecesLeftAreCheckedAgainstTheDomain) {
    // #149: x = K - 1/8 moved right by 1/4 is beyond K exactly.
    expectFailure(
        offset(polyline({{{kLimit - 0.125, 0.0}, 0.0}, {{kLimit - 0.125, 16.0}, 0.0}}), -0.25),
        {g::OffsetFailure::beyondDomain});
    // #153 with a polyline: the only piece collapses inside its band, so it is
    // dropped (no piece left: notRepresentable) or kept (beyondDomain).
    constexpr double kT = 0x1.8p-33;
    expectFailure(offset(polyline({{{kLimit, 0.0}, 0.0}, {{kLimit - 0.125, kT}, 0.0}}), -0x1p26),
                  {g::OffsetFailure::notRepresentable, g::OffsetFailure::beyondDomain});
    // #154: if the first piece is dropped, its exact start beyond K does not
    // count, and the one piece left starts at (K, 2^26).
    const auto joined = offset(polyline({{{kLimit, 0.0}, 0.0},
                                         {{kLimit - 0.125, kT}, 0.0},
                                         {{kLimit - 0.125 - 0x1p17, kT * (1.0 + 0x1p20)}, 0.0}}),
                               -0x1p26);
    if (joined.outcome == g::OffsetOutcome::done) {
        EXPECT_FALSE(joined.simple);
        ASSERT_EQ(joined.polyline.vertices.size(), 2U);
        EXPECT_EQ(joined.polyline.vertices[0].position.x, kLimit);
        EXPECT_EQ(joined.polyline.vertices[0].position.y, 0x1p26);
        ASSERT_EQ(joined.origins.size(), 1U);
        EXPECT_EQ(joined.origins[0].origin, g::PieceOrigin::source);
        EXPECT_EQ(joined.origins[0].index, 1U);
    } else {
        expectFailure(joined, {g::OffsetFailure::beyondDomain});
    }
}

TEST(OffsetPolyline, StraightPiecesCollapsedByRoundingGoWithTheirOrigins) {
    // #148: the piece (0, 0)-(2^-60, 2^-60) moved by 1, inside its band.
    const g::Point2 a{0.0, 0.0};
    const g::Point2 b{0x1p-60, 0x1p-60};
    const auto alone = offset(polyline({{a, 0.0}, {b, 0.0}}), 1.0);
    if (alone.outcome != g::OffsetOutcome::done) {
        expectFailure(alone, {g::OffsetFailure::notRepresentable});
    }
    // Between two long pieces: dropped (with simple false and no entry for it)
    // or kept; never a piece reduced to a point.
    const auto between =
        offset(polyline({{{-10.0, -10.0}, 0.0}, {a, 0.0}, {b, 0.0}, {{10.0, 0.0}, 0.0}}), 1.0);
    ASSERT_EQ(between.outcome, g::OffsetOutcome::done);
    const auto& vertices = between.polyline.vertices;
    ASSERT_EQ(between.origins.size() + 1U, vertices.size());
    for (std::size_t k = 0; k + 1 < vertices.size(); ++k) {
        EXPECT_FALSE(vertices[k].position.x == vertices[k + 1].position.x &&
                     vertices[k].position.y == vertices[k + 1].position.y)
            << "piece " << k;
    }
    const bool dropped = std::ranges::none_of(between.origins, [](const g::OffsetPiece& piece) {
        return piece.origin == g::PieceOrigin::source && piece.index == 1U;
    });
    if (dropped) {
        EXPECT_FALSE(between.simple);
    }
}

// The exact circle of a curved input piece, and the radial gap of a point
// from it once moved by `inward`, from exact numbers (an oracle independent
// of the code under test).
struct ExactCircle {
    Dyadic cx;
    Dyadic cy;
    Dyadic w;
    Dyadic rho;
};

ExactCircle exactCircleOf(const g::PolylineSegment& piece) {
    const auto meaning = detail::meaningOf(piece);
    const auto& circle = std::get<detail::ArcMeaning>(meaning.support).circle;
    return {circle.centerX, circle.centerY, circle.weight, circle.radiusSquared};
}

double gapTo(g::Point2 p, const ExactCircle& circle) {
    const Dyadic dx = Dyadic(p.x) * circle.w - circle.cx;
    const Dyadic dy = Dyadic(p.y) * circle.w - circle.cy;
    const Dyadic power = dx * dx + dy * dy;
    const Dyadic squaredWeight = circle.w * circle.w;
    const double distance = std::sqrt(exact::quotientToDouble(power, squaredWeight));
    const double radius = std::sqrt(exact::quotientToDouble(circle.rho, squaredWeight));
    return exact::quotientToDouble(power - circle.rho, squaredWeight * Dyadic(distance + radius));
}

// Points of a returned arc: by its centre near a whole turn, otherwise in the
// frame of its chord.
std::vector<g::Point2> pointsOf(const g::PolylineSegment& piece, int count) {
    std::vector<g::Point2> points;
    const double phi = 4.0 * std::atan(piece.bulge);
    const double dx = piece.end.x - piece.start.x;
    const double dy = piece.end.y - piece.start.y;
    for (int k = 0; k <= count; ++k) {
        const double fraction = static_cast<double>(k) / count;
        const double scale = std::sin(phi * fraction / 2.0) / std::sin(phi / 2.0);
        const double angle = phi * (fraction - 1.0) / 2.0;
        const double zx = scale * std::cos(angle);
        const double zy = scale * std::sin(angle);
        points.push_back({piece.start.x + dx * zx - dy * zy, piece.start.y + dx * zy + dy * zx});
    }
    return points;
}

double largestCoordinate(g::Point2 p) {
    return std::max(std::fabs(p.x), std::fabs(p.y));
}

// An arc in coordinates centred on a chosen point: centre, radius, start angle
// and signed sweep. The tract oracle below compares two such arcs point by
// point, not only their circles, so that an arc on the right circle but cut
// at the wrong point or running the other way round fails.
struct LocalArc {
    g::Point2 center;
    double radius = 0.0;
    double start = 0.0;
    double sweep = 0.0;
};

g::Point2 pointAt(const LocalArc& arc, double fraction) {
    const double angle = arc.start + arc.sweep * fraction;
    return {arc.center.x + arc.radius * std::cos(angle),
            arc.center.y + arc.radius * std::sin(angle)};
}

// The distance of p from the arc: radial within its sweep, otherwise to the
// nearer end.
double distanceToArc(g::Point2 p, const LocalArc& arc) {
    const double dx = p.x - arc.center.x;
    const double dy = p.y - arc.center.y;
    double along = std::copysign(1.0, arc.sweep) * (std::atan2(dy, dx) - arc.start);
    along = std::fmod(along, 2.0 * kPi);
    if (along < 0.0) {
        along += 2.0 * kPi;
    }
    if (along <= std::fabs(arc.sweep)) {
        return std::fabs(std::hypot(dx, dy) - arc.radius);
    }
    const auto first = pointAt(arc, 0.0);
    const auto last = pointAt(arc, 1.0);
    return std::min(std::hypot(p.x - first.x, p.y - first.y),
                    std::hypot(p.x - last.x, p.y - last.y));
}

// The two-sided distance of two arcs, from samples of each.
double tractDistance(const LocalArc& a, const LocalArc& b) {
    constexpr int kTractSamples = 256;
    double largest = 0.0;
    for (int k = 0; k <= kTractSamples; ++k) {
        const double fraction = static_cast<double>(k) / kTractSamples;
        largest = std::max({largest, distanceToArc(pointAt(a, fraction), b),
                            distanceToArc(pointAt(b, fraction), a)});
    }
    return largest;
}

// The arc of a bulge between two points, in the same coordinates.
LocalArc arcOfBulge(g::Point2 start, g::Point2 end, double bulge) {
    const double vx = end.x - start.x;
    const double vy = end.y - start.y;
    const double chord = std::hypot(vx, vy);
    const double toCenter = (1.0 - bulge * bulge) / (4.0 * bulge);
    LocalArc arc;
    arc.center = {(start.x + end.x) / 2.0 - vy * toCenter, (start.y + end.y) / 2.0 + vx * toCenter};
    arc.radius = chord * (1.0 + bulge * bulge) / (4.0 * std::fabs(bulge));
    arc.start = std::atan2(start.y - arc.center.y, start.x - arc.center.x);
    arc.sweep = 4.0 * std::atan(bulge);
    return arc;
}

TEST(OffsetPolyline, CutAndExtendedArcsKeepTheirEndsAndTheirTract) {
    // An arc of centre O from O + (R, 0) to O + (c, +-s) followed by a
    // segment along an integer direction U with |U| = n, offset by d = q n:
    // the arc is cut or extended to the joint J where the offset line meets
    // the offset circle, independently of the code under test:
    //   w = b + (d / n) (-U.y, U.x) - O,  beta = w . U,
    //   Delta = beta^2 - n^2 (|w|^2 - R'^2),  J - O = w + tau U,
    //   tau = (-beta + sign(beta) sqrt(Delta)) / n^2 (the common point nearest
    //   the vertex b), with R' = R -+ d. Every value is a dyadic number but
    //   sqrt(Delta), so the errors of the returned ends are computed exactly
    //   (exact::quadraticToDouble), and the exact arc runs from the moved start
    //   by the input sweep plus the shortest rotation from its moved end to J.
    // The returned ends must be within 2^-51 max(M, |p|) per coordinate, and
    // every point of either arc within E(A) of the other arc, with S(A) over
    // the exact final piece (#181). Some extensions go far beyond the data and
    // are placed so that they come back near the origin.
    struct Direction {
        int x;
        int y;
        int norm;
    };

    static constexpr std::array<Direction, 20> kDirections{
        {{3, 4, 5},     {4, 3, 5},   {-3, 4, 5},    {-4, 3, 5},   {3, -4, 5},
         {4, -3, 5},    {-3, -4, 5}, {-4, -3, 5},   {0, 1, 1},    {1, 0, 1},
         {0, -1, 1},    {-1, 0, 1},  {5, 12, 13},   {-12, 5, 13}, {12, -5, 13},
         {-5, -12, 13}, {7, 24, 25}, {-24, -7, 25}, {24, -7, 25}, {-7, 24, 25}}};

    // Bulge t = tan(theta / 4) and the end R (cos theta, sin theta) of the
    // arc from (R, 0): integers with c^2 + s^2 = R^2; `unit` scales d.
    struct Sweep {
        double bulge;
        int radius;
        int c;
        int s;
        double unit;
    };

    static constexpr std::array<Sweep, 5> kSweeps{{{0.25, 289, 161, 240, 8.0},
                                                   {0.5, 25, -7, 24, 1.0},
                                                   {0.75, 625, -527, 336, 16.0},
                                                   {1.0, 25, -25, 0, 1.0},
                                                   {2.0, 25, -7, -24, 1.0}}};
    const auto policy = millimetres();
    int cut = 0;
    int extended = 0;
    int farAndBack = 0;
    for (const auto& sweep : kSweeps) {
        const double theta = 4.0 * std::atan(sweep.bulge);
        for (const double turn : {1.0, -1.0}) {
            for (const auto& direction : kDirections) {
                for (const double q : {0.25, -0.25, 1.0, -1.0, 1.5, -1.5, 2.5, -2.5}) {
                    for (const double k : {1.0, 0x1p14}) {
                        const double radius = sweep.radius * k;
                        const double n = direction.norm;
                        const double ux = direction.x;
                        const double uy = direction.y;
                        const double dOverN = q * sweep.unit * k;
                        const double distance = dOverN * n;
                        const double movedRadius = radius - turn * distance;
                        if (std::fabs(distance) >= 0.9 * radius || movedRadius <= 0.05 * radius) {
                            continue;
                        }
                        // The oracle, in coordinates centred on O.
                        const double bx = sweep.c * k;
                        const double by = turn * sweep.s * k;
                        const Dyadic wx = Dyadic(bx) - Dyadic(dOverN) * Dyadic(uy);
                        const Dyadic wy = Dyadic(by) + Dyadic(dOverN) * Dyadic(ux);
                        const Dyadic beta = wx * Dyadic(ux) + wy * Dyadic(uy);
                        const Dyadic nn(n * n);
                        const Dyadic delta =
                            beta * beta -
                            nn * (wx * wx + wy * wy - Dyadic(movedRadius) * Dyadic(movedRadius));
                        if (delta.sign() != r::Sign::positive || beta.isZero()) {
                            continue;
                        }
                        const Dyadic sigma(beta.sign() == r::Sign::positive ? 1.0 : -1.0);
                        const double tau = exact::quadraticToDouble(-beta, sigma, delta, nn);
                        // The segment ends at b + L U: J must stay before its moved end.
                        const double length = std::ceil(3.0 * sweep.radius / n) * k;
                        if (tau >= 0.9 * length) {
                            continue;
                        }
                        const g::Point2 joint{
                            exact::quadraticToDouble(nn * wx - beta * Dyadic(ux),
                                                     sigma * Dyadic(ux), delta, nn),
                            exact::quadraticToDouble(nn * wy - beta * Dyadic(uy),
                                                     sigma * Dyadic(uy), delta, nn)};
                        const g::Point2 movedEnd{movedRadius * bx / radius,
                                                 movedRadius * by / radius};
                        const double cross = movedEnd.x * joint.y - movedEnd.y * joint.x;
                        const double dot = movedEnd.x * joint.x + movedEnd.y * joint.y;
                        const double rotation = turn * std::atan2(cross, dot);
                        const double exactSweep = theta + rotation;
                        if (std::fabs(rotation) > kPi - 1e-6 || exactSweep <= 1e-3 ||
                            exactSweep >= 2.0 * kPi - 0.26) {
                            continue;
                        }
                        const LocalArc exactArc{{0.0, 0.0}, movedRadius, 0.0, turn * exactSweep};
                        std::vector<g::Point2> origins{{0.0, 0.0}, {1e6, 1e6}};
                        const bool far = rotation > 1.0;
                        if (far) {
                            // The middle of the extension at the origin.
                            const auto middle =
                                pointAt(exactArc, (theta + rotation / 2.0) / exactSweep);
                            origins.push_back({-std::round(middle.x), -std::round(middle.y)});
                        }
                        for (std::size_t place = 0; place < origins.size(); ++place) {
                            const auto o = origins[place];
                            const g::Point2 a{o.x + radius, o.y};
                            const g::Point2 b{o.x + bx, o.y + by};
                            const g::Point2 c{b.x + length * ux, b.y + length * uy};
                            const auto input =
                                polyline({{a, turn * sweep.bulge}, {b, 0.0}, {c, 0.0}});
                            const auto result = g::offsetPolyline(input, distance, policy);
                            ASSERT_TRUE(result.hasValue());
                            if (result.value().outcome != g::OffsetOutcome::done) {
                                continue;
                            }
                            const auto& origin = result.value().origins;
                            const auto& vertices = result.value().polyline.vertices;
                            ASSERT_EQ(origin.size(), 2U);
                            ASSERT_EQ(origin[0].origin, g::PieceOrigin::source);
                            ASSERT_EQ(origin[0].index, 0U);
                            ASSERT_EQ(origin[1].origin, g::PieceOrigin::source);
                            ASSERT_EQ(origin[1].index, 1U);
                            const double magnitude = magnitudeOf(input, distance);
                            const auto returnedStart = vertices[0].position;
                            const auto returnedJoint = vertices[1].position;
                            const auto endBound = [&](g::Point2 p) {
                                return 0x1p-51 * std::max(magnitude, largestCoordinate(p));
                            };
                            // The moved start O + (R', 0), exactly.
                            EXPECT_LE(std::fabs((Dyadic(returnedStart.x) - Dyadic(o.x) -
                                                 Dyadic(movedRadius))
                                                    .toDouble()),
                                      endBound(returnedStart));
                            EXPECT_LE(std::fabs((Dyadic(returnedStart.y) - Dyadic(o.y)).toDouble()),
                                      endBound(returnedStart));
                            // The joint: n^2 (x - J.x) = n^2 (x - O.x - w.x) + beta U.x
                            // - sign(beta) U.x sqrt(Delta), exactly.
                            const double jointErrorX = exact::quadraticToDouble(
                                nn * (Dyadic(returnedJoint.x) - Dyadic(o.x) - wx) +
                                    beta * Dyadic(ux),
                                -(sigma * Dyadic(ux)), delta, nn);
                            const double jointErrorY = exact::quadraticToDouble(
                                nn * (Dyadic(returnedJoint.y) - Dyadic(o.y) - wy) +
                                    beta * Dyadic(uy),
                                -(sigma * Dyadic(uy)), delta, nn);
                            EXPECT_LE(std::fabs(jointErrorX), endBound(returnedJoint))
                                << "joint x, place " << place << " rotation " << rotation;
                            EXPECT_LE(std::fabs(jointErrorY), endBound(returnedJoint))
                                << "joint y, place " << place << " rotation " << rotation;
                            // The tract, against E(A) with S(A) over the exact piece.
                            double largest = 0.0;
                            for (int s = 0; s <= 256; ++s) {
                                const auto p = pointAt(exactArc, s / 256.0);
                                largest =
                                    std::max(largest, largestCoordinate({p.x + o.x, p.y + o.y}));
                            }
                            const double bound = 0x1p-46 * std::max(magnitude, largest);
                            const auto returned = arcOfBulge(
                                {returnedStart.x - o.x, returnedStart.y - o.y},
                                {returnedJoint.x - o.x, returnedJoint.y - o.y}, vertices[0].bulge);
                            EXPECT_LE(tractDistance(returned, exactArc), bound)
                                << "tract, place " << place << " rotation " << rotation << " bulge "
                                << vertices[0].bulge;
                            if (rotation < 0.0) {
                                ++cut;
                            } else {
                                ++extended;
                            }
                            if (place == 2) {
                                ++farAndBack;
                            }
                        }
                    }
                }
            }
        }
    }
    EXPECT_GT(cut, 200);
    EXPECT_GT(extended, 200);
    EXPECT_GT(farAndBack, 8);
}

TEST(OffsetPolyline, GapArcsStayOnTheirCircle) {
    // The reversals of r19-giunzione-semicircolare-*, turned and scaled: the
    // gap arc is a half circle of radius |d| around the vertex.
    const auto policy = millimetres();
    for (const double origin : {0.0, 1e6}) {
        for (const double angle : {0.0, 0.3, 1.9}) {
            const g::Point2 v{origin + 20.0 * std::cos(angle), origin + 20.0 * std::sin(angle)};
            const auto input =
                polyline({{{origin, origin}, 0.0}, {v, 0.0}, {{origin, origin}, 0.0}});
            for (const double distance : {2.0, -2.0, 0.5}) {
                const auto result = g::offsetPolyline(input, distance, policy);
                ASSERT_TRUE(result.hasValue());
                ASSERT_EQ(result.value().outcome, g::OffsetOutcome::done);
                const auto& vertices = result.value().polyline.vertices;
                ASSERT_EQ(result.value().origins.size(), 3U);
                ASSERT_EQ(result.value().origins[1].origin, g::PieceOrigin::gap);
                const g::PolylineSegment gap{vertices[1].position, vertices[2].position,
                                             vertices[1].bulge};
                const double magnitude = magnitudeOf(input, distance);
                const double bound = 0x1p-46 * std::max(magnitude, largestCoordinate(v) + 2.0);
                // The exact circle of the gap: centre v, radius |d|.
                const ExactCircle circle{Dyadic(v.x), Dyadic(v.y), Dyadic(1.0),
                                         Dyadic(distance) * Dyadic(distance)};
                for (const auto p : pointsOf(gap, 64)) {
                    EXPECT_LE(std::fabs(gapTo(p, circle)), bound);
                }
                EXPECT_NEAR(gap.bulge, distance > 0.0 ? -1.0 : 1.0,
                            4.0 * bound / std::fabs(2.0 * distance));
            }
        }
    }
}

TEST(OffsetPolyline, NearlyWholeArcsOfIssues127And132) {
    constexpr double kUlp = 0x1p-33;
    for (const auto& input :
         {polyline({{{1e6, 0.0}, 0x1p32}, {{1e6 + kUlp, 0.0}, 0.0}}),
          polyline({{{1e6, 1e6}, 0x1p32}, {{1e6 + kUlp, 1e6 + 2.0 * kUlp}, 0.0}})}) {
        const auto result = offset(input, -0.03125);
        if (result.outcome != g::OffsetOutcome::done) {
            expectFailure(result, {g::OffsetFailure::notRepresentable});
            continue;
        }
        EXPECT_TRUE(result.simple);
        ASSERT_EQ(result.polyline.vertices.size(), 2U);
        const auto circle = exactCircleOf(g::pieces(input)[0]);
        const auto& vertices = result.polyline.vertices;
        const g::PolylineSegment arc{vertices[0].position, vertices[1].position, vertices[0].bulge};
        // Points of a nearly whole arc, by its own centre and radius.
        const auto meaning = detail::meaningOf(arc);
        const auto& own = std::get<detail::ArcMeaning>(meaning.support).circle;
        const double cx = exact::quotientToDouble(own.centerX, own.weight);
        const double cy = exact::quotientToDouble(own.centerY, own.weight);
        const double radius =
            std::sqrt(exact::quotientToDouble(own.radiusSquared, own.weight * own.weight));
        EXPECT_NEAR(radius, 0.15625, 1e-3);
        const double bound = 0x1p-46 * (1e6 + 1.0);
        for (int k = 0; k < 64; ++k) {
            const double angle = 2.0 * kPi * k / 64.0;
            const g::Point2 p{cx + radius * std::cos(angle), cy + radius * std::sin(angle)};
            EXPECT_LE(std::fabs(gapTo(p, circle) - 0.03125), bound);
        }
    }
}

TEST(OffsetPolyline, AnArcWhoseRoundedEndsCoincideIsNotDroppedAsAPoint) {
    // The arc of OffsetCurve.AnArcWhoseRoundedEndsCoincideFails: a nonzero
    // exact arc whose rounded ends coincide gives notRepresentable, not a
    // point dropped by step 4 (#132).
    const double radius = 0x1p-14 + 0x1p-54;
    const auto result =
        offset(polyline({{{1e6, 1e6 - 0x1p-33}, 0x1p20}, {{1e6, 1e6 + 0x1p-33}, 0.0}}),
               radius * 15.0 / 16.0);
    expectFailure(result, {g::OffsetFailure::notRepresentable});
}

TEST(OffsetPolyline, OnePieceAgreesWithOffsetCurveBitForBit) {
    Generator generator(19);
    for (int trial = 0; trial < 200; ++trial) {
        const double scale = trial % 2 == 0 ? 1.0 : 1e6;
        const g::Point2 start{scale + generator.between(-40, 40) / 8.0,
                              generator.between(-40, 40) / 8.0};
        const g::Point2 end{start.x + generator.between(1, 40) / 4.0,
                            start.y + generator.between(-40, 40) / 4.0};
        const double bulge = generator.between(-24, 24) / 8.0;
        const double distance = generator.between(-16, 16) / 16.0;
        const g::PolylineSegment piece{start, end, bulge};
        const auto curve = g::offsetCurve(piece, distance);
        const auto poly =
            g::offsetPolyline(polyline({{start, bulge}, {end, 0.0}}), distance, millimetres());
        ASSERT_TRUE(curve.hasValue());
        ASSERT_TRUE(poly.hasValue());
        if (curve.value().outcome != g::OffsetOutcome::done ||
            poly.value().outcome != g::OffsetOutcome::done) {
            continue;
        }
        const auto& moved = std::get<g::PolylineSegment>(curve.value().curve);
        const auto& vertices = poly.value().polyline.vertices;
        ASSERT_EQ(vertices.size(), 2U);
        EXPECT_EQ(std::bit_cast<std::uint64_t>(vertices[0].position.x),
                  std::bit_cast<std::uint64_t>(moved.start.x));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(vertices[0].position.y),
                  std::bit_cast<std::uint64_t>(moved.start.y));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(vertices[1].position.x),
                  std::bit_cast<std::uint64_t>(moved.end.x));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(vertices[1].position.y),
                  std::bit_cast<std::uint64_t>(moved.end.y));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(vertices[0].bulge),
                  std::bit_cast<std::uint64_t>(moved.bulge));
    }
}

}  // namespace
