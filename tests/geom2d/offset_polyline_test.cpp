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
#include <limits>
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

}  // namespace
