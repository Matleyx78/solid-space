// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Unit tests of the exact meaning of the curves (stage 0.2, package P7). The
// directions of the arcs are built by hand, not through robust::sinCos, so
// that the four rules of Arc are checked at their borders; the expected
// answers follow from the definitions in curves.hpp, never from running the
// code under test.

#include "curve_meaning.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <variant>

namespace {

using solid_space::geom2d::Arc;
using solid_space::geom2d::Circle;
using solid_space::geom2d::DiagnosisCode;
using solid_space::geom2d::kFullTurn;
using solid_space::geom2d::kQuarterTurn;
using solid_space::geom2d::kThreeQuarterTurn;
using solid_space::geom2d::Location;
using solid_space::geom2d::Point2;
using solid_space::geom2d::PolylineSegment;
using solid_space::geom2d::Segment;
using solid_space::geom2d::detail::ArcMeaning;
using solid_space::geom2d::detail::ArcSpan;
using solid_space::geom2d::detail::Dyadic;
using solid_space::geom2d::detail::ExactVector;
using solid_space::geom2d::detail::Turn;
using solid_space::geom2d::detail::TurnSector;
using solid_space::robust::Sign;

namespace detail = solid_space::geom2d::detail;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInfinity = std::numeric_limits<double>::infinity();
constexpr double kTiny = 1e-300;
constexpr double kMaxCoordinate = 0x1p50;

double below(double value) {
    return std::nextafter(value, 0.0);
}

double above(double value) {
    return std::nextafter(value, kInfinity);
}

// SplitMix64: a deterministic stream, the same on every platform.
class Generator {
public:
    explicit Generator(std::uint64_t seed) : m_state(seed) {}

    std::uint64_t next() {
        m_state += 0x9E3779B97F4A7C15ULL;
        std::uint64_t z = m_state;
        z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31U);
    }

    // A double in [-limit, limit), with 53 random bits.
    double uniform(double limit) {
        const double unit = static_cast<double>(next() >> 11U) * 0x1p-53;
        return (2.0 * unit - 1.0) * limit;
    }

private:
    std::uint64_t m_state;
};

ExactVector vec(double x, double y) {
    return ExactVector{Dyadic(x), Dyadic(y)};
}

const ArcMeaning& arcOf(const detail::CurveMeaning& meaning) {
    return std::get<ArcMeaning>(meaning.support);
}

// --- The four rules of Arc ------------------------------------------------

TEST(ArcRule, WholeTurnOrMoreIsTheWholeCircle) {
    // D1 just behind D0 (alpha just under 2 pi): whole from kFullTurn on.
    const Point2 d0{1.0, 0.0};
    const Point2 behind{1.0, -kTiny};
    EXPECT_EQ(detail::arcRule(kFullTurn, d0, behind).span, ArcSpan::whole);
    EXPECT_EQ(detail::arcRule(1e6, d0, behind).span, ArcSpan::whole);
    const auto justBelow = detail::arcRule(below(kFullTurn), d0, behind);
    EXPECT_EQ(justBelow.span, ArcSpan::partial);
    EXPECT_EQ(justBelow.endSector, TurnSector::fourth);
    // The same, clockwise: behind D0 is now counterclockwise of it.
    const Point2 behindClockwise{1.0, kTiny};
    EXPECT_EQ(detail::arcRule(-kFullTurn, d0, behindClockwise).span, ArcSpan::whole);
    const auto clockwise = detail::arcRule(-below(kFullTurn), d0, behindClockwise);
    EXPECT_EQ(clockwise.span, ArcSpan::partial);
    EXPECT_EQ(clockwise.turn, Turn::clockwise);
    EXPECT_EQ(clockwise.endSector, TurnSector::fourth);
}

TEST(ArcRule, D1PastD0ThroughRoundingIsTheWholeCircle) {
    const Point2 d0{1.0, 0.0};
    // alpha == 0, inside the first quarter, exactly pi / 2: whole from
    // kThreeQuarterTurn on.
    for (const Point2 d1 : {Point2{1.0, 0.0}, Point2{2.0, kTiny}, Point2{0.0, 3.0}}) {
        EXPECT_EQ(detail::arcRule(kThreeQuarterTurn, d0, d1).span, ArcSpan::whole);
        EXPECT_EQ(detail::arcRule(above(kThreeQuarterTurn), d0, d1).span, ArcSpan::whole);
    }
    // Just past pi / 2 the rule does not apply: the arc is alpha.
    const auto past = detail::arcRule(kThreeQuarterTurn, d0, Point2{-kTiny, 1.0});
    EXPECT_EQ(past.span, ArcSpan::partial);
    EXPECT_EQ(past.endSector, TurnSector::second);
    // Just below kThreeQuarterTurn it does not apply either.
    const auto short1 = detail::arcRule(below(kThreeQuarterTurn), d0, Point2{0.0, 1.0});
    EXPECT_EQ(short1.span, ArcSpan::partial);
    EXPECT_EQ(short1.endSector, TurnSector::quarter);
    const auto short2 = detail::arcRule(below(kThreeQuarterTurn), d0, Point2{1.0, 0.0});
    EXPECT_EQ(short2.span, ArcSpan::point);
    // Clockwise: pi / 2 clockwise is (0, -1).
    EXPECT_EQ(detail::arcRule(-kThreeQuarterTurn, d0, Point2{0.0, -1.0}).span, ArcSpan::whole);
    EXPECT_EQ(detail::arcRule(-kThreeQuarterTurn, d0, Point2{0.0, 1.0}).span, ArcSpan::partial);
}

TEST(ArcRule, D1BehindD0ThroughRoundingIsAPoint) {
    const Point2 d0{0.0, 1.0};
    // Counterclockwise from (0, 1), alpha is exactly 3 pi / 2 at (1, 0) and
    // inside the fourth quarter at (kTiny, 1): a point up to kQuarterTurn.
    for (const Point2 d1 : {Point2{1.0, 0.0}, Point2{kTiny, 1.0}}) {
        EXPECT_EQ(detail::arcRule(kQuarterTurn, d0, d1).span, ArcSpan::point);
        EXPECT_EQ(detail::arcRule(1e-12, d0, d1).span, ArcSpan::point);
        const auto longer = detail::arcRule(above(kQuarterTurn), d0, d1);
        EXPECT_EQ(longer.span, ArcSpan::partial);
    }
    EXPECT_EQ(detail::arcRule(above(kQuarterTurn), d0, Point2{1.0, 0.0}).endSector,
              TurnSector::threeQuarters);
    EXPECT_EQ(detail::arcRule(above(kQuarterTurn), d0, Point2{kTiny, 1.0}).endSector,
              TurnSector::fourth);
    // Inside the third quarter the rule does not apply.
    const auto third = detail::arcRule(kQuarterTurn, d0, Point2{1.0, -kTiny});
    EXPECT_EQ(third.span, ArcSpan::partial);
    EXPECT_EQ(third.endSector, TurnSector::third);
    // Clockwise, 3 pi / 2 from (0, 1) is (-1, 0), and (1, 0) is a quarter.
    EXPECT_EQ(detail::arcRule(-kQuarterTurn, d0, Point2{-1.0, 0.0}).span, ArcSpan::point);
    const auto quarter = detail::arcRule(-kQuarterTurn, d0, Point2{1.0, 0.0});
    EXPECT_EQ(quarter.span, ArcSpan::partial);
    EXPECT_EQ(quarter.endSector, TurnSector::quarter);
}

TEST(ArcRule, OtherwiseTheArcIsAlpha) {
    const Point2 d0{3.0, 4.0};
    // alpha == 0 is again a point, whatever the length of the vectors.
    EXPECT_EQ(detail::arcRule(1.0, d0, Point2{6.0, 8.0}).span, ArcSpan::point);
    EXPECT_EQ(detail::arcRule(0.0, d0, d0).span, ArcSpan::point);
    EXPECT_EQ(detail::arcRule(-0.0, d0, d0).span, ArcSpan::point);

    struct Expected {
        Point2 d1;
        TurnSector counterclockwise = TurnSector::zero;
        TurnSector clockwise = TurnSector::zero;
    };

    for (const auto& [d1, ccw, cw] : {
             Expected{{4.0, 3.0}, TurnSector::fourth, TurnSector::first},
             Expected{{-4.0, 3.0}, TurnSector::quarter, TurnSector::threeQuarters},
             Expected{{-4.0, -3.0}, TurnSector::second, TurnSector::third},
             Expected{{-3.0, -4.0}, TurnSector::half, TurnSector::half},
             Expected{{4.0, -3.0}, TurnSector::threeQuarters, TurnSector::quarter},
         }) {
        const auto forward = detail::arcRule(2.0, d0, d1);
        EXPECT_EQ(forward.span, ArcSpan::partial);
        EXPECT_EQ(forward.turn, Turn::counterclockwise);
        EXPECT_EQ(forward.endSector, ccw);
        const auto backward = detail::arcRule(-2.0, d0, d1);
        EXPECT_EQ(backward.span, ArcSpan::partial);
        EXPECT_EQ(backward.turn, Turn::clockwise);
        EXPECT_EQ(backward.endSector, cw);
    }
}

// --- Sectors and order -----------------------------------------------------

TEST(TurnSector, SignsGiveTheSector) {
    using S = Sign;
    EXPECT_EQ(detail::sectorOf(S::zero, S::positive), TurnSector::zero);
    EXPECT_EQ(detail::sectorOf(S::positive, S::positive), TurnSector::first);
    EXPECT_EQ(detail::sectorOf(S::positive, S::zero), TurnSector::quarter);
    EXPECT_EQ(detail::sectorOf(S::positive, S::negative), TurnSector::second);
    EXPECT_EQ(detail::sectorOf(S::zero, S::negative), TurnSector::half);
    EXPECT_EQ(detail::sectorOf(S::negative, S::negative), TurnSector::third);
    EXPECT_EQ(detail::sectorOf(S::negative, S::zero), TurnSector::threeQuarters);
    EXPECT_EQ(detail::sectorOf(S::negative, S::positive), TurnSector::fourth);
    EXPECT_EQ(detail::oriented(Turn::clockwise, S::positive), S::negative);
    EXPECT_EQ(detail::oriented(Turn::counterclockwise, S::negative), S::negative);
}

TEST(TurnSector, CompareTurnsUsesTheCrossOnlyInsideAQuarter) {
    EXPECT_EQ(detail::compareTurns(TurnSector::first, TurnSector::second, Sign::negative),
              Sign::negative);
    EXPECT_EQ(detail::compareTurns(TurnSector::half, TurnSector::quarter, Sign::positive),
              Sign::positive);
    EXPECT_EQ(detail::compareTurns(TurnSector::half, TurnSector::half, Sign::positive), Sign::zero);
    // Same quarter: u first exactly when v turns from u in the travel sense.
    EXPECT_EQ(detail::compareTurns(TurnSector::third, TurnSector::third, Sign::positive),
              Sign::negative);
    EXPECT_EQ(detail::compareTurns(TurnSector::third, TurnSector::third, Sign::negative),
              Sign::positive);
    EXPECT_EQ(detail::compareTurns(TurnSector::third, TurnSector::third, Sign::zero), Sign::zero);
}

// --- Directions on an arc --------------------------------------------------

TEST(Direction, CounterclockwiseQuarterArc) {
    // From (1, 0) to (0, 1) counterclockwise, built from the rules.
    ArcMeaning arc;
    arc.turn = Turn::counterclockwise;
    arc.span = ArcSpan::partial;
    arc.startDirection = vec(1.0, 0.0);
    arc.endDirection = vec(0.0, 1.0);
    arc.endSector = TurnSector::quarter;
    EXPECT_EQ(detail::locateDirection(arc, vec(5.0, 0.0)), Location::atStart);
    EXPECT_EQ(detail::locateDirection(arc, vec(1.0, 1.0)), Location::inside);
    EXPECT_EQ(detail::locateDirection(arc, vec(1.0, kTiny)), Location::inside);
    EXPECT_EQ(detail::locateDirection(arc, vec(kTiny, 1.0)), Location::inside);
    EXPECT_EQ(detail::locateDirection(arc, vec(0.0, 0.5)), Location::atEnd);
    EXPECT_EQ(detail::locateDirection(arc, vec(-kTiny, 1.0)), Location::afterEnd);
    EXPECT_EQ(detail::locateDirection(arc, vec(-1.0, 0.0)), Location::afterEnd);
    EXPECT_EQ(detail::locateDirection(arc, vec(1.0, -kTiny)), Location::afterEnd);
    // Order from D0: (1, 1) before (0, 1) before (-1, 0) before (1, -1).
    EXPECT_EQ(detail::compareAlongArc(arc, vec(1.0, 1.0), vec(0.0, 1.0)), Sign::negative);
    EXPECT_EQ(detail::compareAlongArc(arc, vec(-1.0, 0.0), vec(0.0, 1.0)), Sign::positive);
    EXPECT_EQ(detail::compareAlongArc(arc, vec(1.0, -1.0), vec(-1.0, 0.0)), Sign::positive);
    EXPECT_EQ(detail::compareAlongArc(arc, vec(2.0, 1.0), vec(1.0, 2.0)), Sign::negative);
    EXPECT_EQ(detail::compareAlongArc(arc, vec(2.0, 2.0), vec(1.0, 1.0)), Sign::zero);
}

TEST(Direction, ClockwiseArcOverHalfATurn) {
    // From (0, 1) clockwise by 3 pi / 2 to (-1, 0).
    ArcMeaning arc;
    arc.turn = Turn::clockwise;
    arc.span = ArcSpan::partial;
    arc.startDirection = vec(0.0, 1.0);
    arc.endDirection = vec(-1.0, 0.0);
    arc.endSector = TurnSector::threeQuarters;
    EXPECT_EQ(detail::locateDirection(arc, vec(0.0, 2.0)), Location::atStart);
    EXPECT_EQ(detail::locateDirection(arc, vec(1.0, 1.0)), Location::inside);
    EXPECT_EQ(detail::locateDirection(arc, vec(0.0, -1.0)), Location::inside);
    EXPECT_EQ(detail::locateDirection(arc, vec(-1.0, -kTiny)), Location::inside);
    EXPECT_EQ(detail::locateDirection(arc, vec(-3.0, 0.0)), Location::atEnd);
    EXPECT_EQ(detail::locateDirection(arc, vec(-1.0, kTiny)), Location::afterEnd);
    EXPECT_EQ(detail::locateDirection(arc, vec(-1.0, 1.0)), Location::afterEnd);
    EXPECT_EQ(detail::compareAlongArc(arc, vec(1.0, 0.0), vec(0.0, -1.0)), Sign::negative);
    EXPECT_EQ(detail::compareAlongArc(arc, vec(-1.0, 1.0), vec(-1.0, -1.0)), Sign::positive);
}

TEST(Direction, WholeArcsAndCircles) {
    const auto whole = detail::meaningOf(Arc{{0.0, 0.0}, 2.0, 0.0, 7.0});
    ASSERT_EQ(arcOf(whole).span, ArcSpan::whole);
    EXPECT_EQ(detail::locateDirection(arcOf(whole), arcOf(whole).startDirection),
              Location::atStart);
    EXPECT_EQ(detail::locateDirection(arcOf(whole), vec(-1.0, 0.0)), Location::inside);
    const auto circle = detail::meaningOf(Circle{{0.0, 0.0}, 2.0});
    ASSERT_EQ(arcOf(circle).span, ArcSpan::circle);
    EXPECT_EQ(detail::locateDirection(arcOf(circle), vec(1.0, 0.0)), Location::inside);
    EXPECT_EQ(detail::locateDirection(arcOf(circle), vec(0.0, -1.0)), Location::inside);
    EXPECT_EQ(detail::compareAlongArc(arcOf(circle), vec(0.0, -1.0), vec(0.0, 1.0)),
              Sign::positive);
}

// --- The circle of a curved polyline segment -------------------------------

void expectOnCircle(const detail::CircleSupport& circle, const ExactVector& point) {
    const Dyadic dx = point.x * circle.weight - circle.centerX;
    const Dyadic dy = point.y * circle.weight - circle.centerY;
    EXPECT_EQ(compare(dx * dx + dy * dy, circle.radiusSquared), Sign::zero);
}

// Coordinates far from the origin, near it, and tiny.
constexpr std::array<double, 3> kScales = {1e6, 1.0, 1e-6};

TEST(PolylineArc, CirclePassesExactlyThroughStartMAndEnd) {
    Generator generator(0x5EED7);
    for (int i = 0; i < 2000; ++i) {
        const double scale = kScales.at(static_cast<std::size_t>(i) % kScales.size());
        const Point2 start{generator.uniform(scale), generator.uniform(scale)};
        const Point2 end{generator.uniform(scale), generator.uniform(scale)};
        const double bulge = i % 5 == 0 ? generator.uniform(1e-8) : generator.uniform(8.0);
        if (bulge == 0.0) {
            continue;
        }
        const PolylineSegment piece{start, end, bulge};
        const auto meaning = detail::meaningOf(piece);
        ASSERT_FALSE(meaning.degenerate);
        const auto& arc = arcOf(meaning);
        EXPECT_EQ(arc.circle.weight.sign(), Sign::positive);
        EXPECT_EQ(arc.span, ArcSpan::partial);
        EXPECT_EQ(arc.turn, bulge > 0.0 ? Turn::counterclockwise : Turn::clockwise);
        const auto m = detail::arcMidpoint(piece);
        expectOnCircle(arc.circle, detail::exactVector(start));
        expectOnCircle(arc.circle, m);
        expectOnCircle(arc.circle, detail::exactVector(end));
        // The arc contains m, strictly inside.
        const ExactVector toM{m.x * arc.circle.weight - arc.circle.centerX,
                              m.y * arc.circle.weight - arc.circle.centerY};
        EXPECT_EQ(detail::locateDirection(arc, toM), Location::inside);
        // More than half a turn exactly when |bulge| > 1.
        const auto half = detail::compareTurns(arc.endSector, TurnSector::half, Sign::zero);
        EXPECT_EQ(half, std::fabs(bulge) > 1.0 ? Sign::positive : Sign::negative);
    }
}

TEST(PolylineArc, HalfCircleAndDegenerate) {
    // bulge 1 over (0, 0)-(2, 0): centre (1, 0), radius 1, through (1, -1).
    const auto half = detail::meaningOf(PolylineSegment{{0.0, 0.0}, {2.0, 0.0}, 1.0});
    const auto& arc = arcOf(half);
    EXPECT_EQ(compare(arc.circle.centerX, arc.circle.weight), Sign::zero);
    EXPECT_TRUE(arc.circle.centerY.isZero());
    EXPECT_EQ(compare(arc.circle.radiusSquared, arc.circle.weight * arc.circle.weight), Sign::zero);
    EXPECT_EQ(arc.endSector, TurnSector::half);
    EXPECT_EQ(half.magnitude, 2.0);
    // The same ends: a point, whatever the bulge.
    const auto point = detail::meaningOf(PolylineSegment{{3.0, 4.0}, {3.0, 4.0}, -2.0});
    EXPECT_TRUE(point.degenerate);
    EXPECT_EQ(arcOf(point).span, ArcSpan::point);
    // bulge -0 is straight.
    const auto straight = detail::meaningOf(PolylineSegment{{0.0, 0.0}, {2.0, 0.0}, -0.0});
    EXPECT_TRUE(std::holds_alternative<detail::LineMeaning>(straight.support));
}

// --- Checking the data -----------------------------------------------------

std::optional<DiagnosisCode> codeOf(const solid_space::geom2d::Curve& curve) {
    const auto problem = detail::check(curve, 3);
    if (!problem) {
        return std::nullopt;
    }
    EXPECT_EQ(problem->input, 3U);
    EXPECT_FALSE(problem->message.empty());
    return problem->code;
}

TEST(Check, DiagnosesInTheOrderOfCurvesHpp) {
    const double beyond = above(kMaxCoordinate);
    // Not finite before out of domain, out of domain before degenerate.
    EXPECT_EQ(codeOf(Segment{{beyond, 0.0}, {kNaN, 0.0}}), DiagnosisCode::invalidValue);
    EXPECT_EQ(codeOf(Arc{{beyond, 0.0}, -1.0, 0.0, 1.0}), DiagnosisCode::invalidValue);
    EXPECT_EQ(codeOf(Arc{{0.0, 0.0}, 1.0, 2e6, kInfinity}), DiagnosisCode::invalidValue);
    EXPECT_EQ(codeOf(Circle{{beyond, 0.0}, 0.0}), DiagnosisCode::outOfDomain);
    EXPECT_EQ(codeOf(PolylineSegment{{beyond, 0.0}, {beyond, 0.0}, 1.0}),
              DiagnosisCode::outOfDomain);
    // The limits themselves are inside the domain.
    EXPECT_EQ(codeOf(Segment{{kMaxCoordinate, 0.0}, {-kMaxCoordinate, 0.0}}), std::nullopt);
    EXPECT_EQ(codeOf(Arc{{0.0, 0.0}, kMaxCoordinate, 0x1p20, -1.0}), std::nullopt);
    EXPECT_EQ(codeOf(Arc{{0.0, 0.0}, 1.0, 0x1p20, 1.0}), DiagnosisCode::outOfDomain);
    EXPECT_EQ(codeOf(Arc{{0.0, 0.0}, 1.0, -above(0x1p20), 10.0}), DiagnosisCode::outOfDomain);
    // A huge sweep puts the end angle beyond the domain.
    EXPECT_EQ(codeOf(Arc{{0.0, 0.0}, 1.0, 0.0, std::numeric_limits<double>::max()}),
              DiagnosisCode::outOfDomain);
    // m beyond the domain, with ends inside.
    EXPECT_EQ(codeOf(PolylineSegment{{-kMaxCoordinate, 0.0}, {kMaxCoordinate, 0.0}, 1.0}),
              std::nullopt);
    EXPECT_EQ(codeOf(PolylineSegment{{-kMaxCoordinate, 0.0}, {kMaxCoordinate, 0.0}, above(1.0)}),
              DiagnosisCode::outOfDomain);
    // Degenerate curves.
    EXPECT_EQ(codeOf(Segment{{1.0, -0.0}, {1.0, 0.0}}), DiagnosisCode::degenerateCurve);
    EXPECT_EQ(codeOf(Circle{{1.0, 2.0}, -0.0}), DiagnosisCode::degenerateCurve);
    EXPECT_EQ(codeOf(Arc{{1.0, 2.0}, 0.0, 0.0, 3.0}), DiagnosisCode::degenerateCurve);
    EXPECT_EQ(codeOf(Arc{{1.0, 2.0}, 1.0, 1e6, 1e-12}), DiagnosisCode::degenerateCurve);
    EXPECT_EQ(codeOf(PolylineSegment{{1.0, 2.0}, {1.0, 2.0}, 0.0}), DiagnosisCode::degenerateCurve);
    // checkData accepts them.
    EXPECT_FALSE(detail::checkData(Arc{{1.0, 2.0}, 1.0, 0.0, 0.0}, 0).has_value());
    const auto accepted =
        detail::checkedMeaning(Circle{{1.0, 2.0}, 0.0}, 1, detail::Degenerate::accept);
    ASSERT_TRUE(accepted.hasValue());
    EXPECT_TRUE(accepted.value().degenerate);
    const auto rejected = detail::checkedMeaning(Circle{{1.0, 2.0}, 0.0}, 1);
    ASSERT_FALSE(rejected.hasValue());
    EXPECT_EQ(rejected.diagnosis().input, 1U);
}

TEST(Check, MagnitudeOfSeveralCurves) {
    const std::array<detail::CurveMeaning, 3> curves = {
        detail::meaningOf(Segment{{-3.0, 4.0}, {2.0, -7.0}}),
        detail::meaningOf(Circle{{-3.0, 4.0}, 9.0}),
        detail::meaningOf(PolylineSegment{{0.0, 0.0}, {2.0, 0.0}, 6.0}),
    };
    EXPECT_EQ(curves[0].magnitude, 7.0);
    EXPECT_EQ(curves[1].magnitude, 9.0);
    // m = (1, 0) + 3 * (0, -2) = (1, -6).
    EXPECT_EQ(curves[2].magnitude, 6.0);
    EXPECT_EQ(detail::magnitudeOf(curves), 9.0);
    EXPECT_EQ(detail::magnitudeOf(std::span<const detail::CurveMeaning>{}), 0.0);
}

}  // namespace
