// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "solid_space/geom2d/coincidence.hpp"

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace {

namespace geom2d = solid_space::geom2d;
using solid_space::robust::TolerancePolicy;

// A fixed integer stream; all generated coordinates and radii are dyadic.
class Generator {
public:
    double coordinate() {
        m_state = m_state * 6364136223846793005ULL + 1442695040888963407ULL;
        return (static_cast<double>(m_state >> 48U) - 32768.0) * 0x1p-10;
    }

private:
    std::uint64_t m_state = 0xC018C1DE;
};

void expectSame(const geom2d::Result<bool>& a, const geom2d::Result<bool>& b) {
    ASSERT_TRUE(a.hasValue());
    ASSERT_TRUE(b.hasValue());
    EXPECT_EQ(a.value(), b.value());
}

void expectTrue(const geom2d::Result<bool>& result) {
    ASSERT_TRUE(result.hasValue());
    EXPECT_TRUE(result.value());
}

TEST(Coincidence, AlignmentIsSymmetricAndIndependentOfEndpointOrder) {
    const auto policy = TolerancePolicy::forUnit(1e-3).value();
    Generator generator;
    constexpr std::array<double, 3> kScales{0x1p-40, 1.0, 0x1p20};
    for (int i = 0; i < 600; ++i) {
        const double scale = kScales.at(static_cast<std::size_t>(i) % kScales.size());
        const geom2d::Point2 start{generator.coordinate() * scale, generator.coordinate() * scale};
        const geom2d::Segment a{start, {start.x + scale, start.y + 2.0 * scale}};
        const double shift = generator.coordinate() * policy.epsilon(scale);
        const geom2d::Segment b{{a.start.x, a.start.y + shift}, {a.end.x, a.end.y + shift}};
        const auto answer = geom2d::areAligned(a, b, policy);
        expectSame(answer, geom2d::areAligned(b, a, policy));
        expectSame(answer, geom2d::areAligned({a.end, a.start}, b, policy));
        expectSame(answer, geom2d::areAligned(a, {b.end, b.start}, policy));
        expectTrue(geom2d::areAligned(a, a, policy));
        expectTrue(geom2d::areAligned(a, {a.end, a.start}, policy));
    }
}

TEST(Coincidence, CircularComparisonsAreSymmetricAcrossCurveRepresentations) {
    const auto policy = TolerancePolicy::forUnit(1.0).value();
    Generator generator;
    for (int i = 0; i < 400; ++i) {
        const geom2d::Point2 center{generator.coordinate(), generator.coordinate()};
        const double radius = 1.0 + static_cast<double>(i % 17);
        // The semicircle has exactly the same centre and radius as the circle.
        const geom2d::Circle circle{center, radius};
        const geom2d::Arc arc{center, radius, 0.25, -2.0};
        const geom2d::Point2 start{center.x - radius, center.y};
        const geom2d::Point2 end{center.x + radius, center.y};
        const geom2d::PolylineSegment semicircle{start, end, 1.0};
        const std::array<geom2d::Curve, 3> curves{circle, arc, semicircle};
        const double shift = generator.coordinate() * policy.floorLength();
        const geom2d::Circle other{{center.x + shift, center.y}, radius + shift};
        for (const auto& a : curves) {
            for (const auto& b : curves) {
                expectTrue(geom2d::areConcentric(a, b, policy));
                expectTrue(geom2d::onSameCircle(a, b, policy));
            }
            expectSame(geom2d::areConcentric(a, other, policy),
                       geom2d::areConcentric(other, a, policy));
            expectSame(geom2d::onSameCircle(a, other, policy),
                       geom2d::onSameCircle(other, a, policy));
        }
        // A non-semicircular piece exercises rational centres and irrational radii.
        const geom2d::PolylineSegment piece{center, {center.x + 1.0, center.y + 2.0}, 0.3};
        const geom2d::PolylineSegment reversed{piece.end, piece.start, -piece.bulge};
        expectTrue(geom2d::onSameCircle(piece, reversed, policy));
        expectSame(geom2d::areConcentric(piece, other, policy),
                   geom2d::areConcentric(other, piece, policy));
        expectSame(geom2d::onSameCircle(piece, other, policy),
                   geom2d::onSameCircle(other, piece, policy));
    }
}

TEST(Coincidence, ExactlyDegenerateCurvesAndStraightPiecesStaySo) {
    const auto policy = TolerancePolicy::forUnit(1000.0).value();
    Generator generator;
    for (int i = 0; i < 200; ++i) {
        const geom2d::Point2 point{generator.coordinate(), generator.coordinate()};
        const std::array<geom2d::Curve, 4> curves{
            geom2d::Segment{point, point}, geom2d::Circle{point, 0.0},
            geom2d::Arc{point, 1.0, 0.5, 0.0}, geom2d::PolylineSegment{point, point, 3.0}};
        for (const auto& curve : curves) {
            expectTrue(geom2d::isDegenerate(curve, policy));
        }
        expectTrue(geom2d::isStraight({point, point, 3.0}, policy));
        expectTrue(geom2d::isStraight({point, {point.x + 1.0, point.y - 2.0}, -0.0}, policy));
    }
}

bool samePoint(geom2d::Point2 a, geom2d::Point2 b) {
    return std::bit_cast<std::uint64_t>(a.x) == std::bit_cast<std::uint64_t>(b.x) &&
           std::bit_cast<std::uint64_t>(a.y) == std::bit_cast<std::uint64_t>(b.y);
}

void expectJoined(const geom2d::Result<geom2d::Segment>& result, geom2d::Point2 start,
                  geom2d::Point2 end) {
    ASSERT_TRUE(result.hasValue()) << result.diagnosis().message;
    EXPECT_TRUE(samePoint(result.value().start, start))
        << result.value().start.x << ", " << result.value().start.y;
    EXPECT_TRUE(samePoint(result.value().end, end))
        << result.value().end.x << ", " << result.value().end.y;
}

void expectDiagnosis(const geom2d::Result<geom2d::Segment>& result, geom2d::DiagnosisCode code,
                     std::size_t input) {
    ASSERT_FALSE(result.hasValue());
    EXPECT_EQ(result.diagnosis().code, code);
    EXPECT_EQ(result.diagnosis().input, input);
}

TEST(JoinAligned, SourceWinsTheTieOfTheExampleOfIssue158) {
    const geom2d::Segment source{{0.0, 0.0}, {10.0, 0.0}};
    const std::array<geom2d::Segment, 1> others{geom2d::Segment{{0.0, 5e-7}, {5.0, 0.0}}};
    expectJoined(geom2d::joinAligned(source, others), {0.0, 0.0}, {10.0, 0.0});
}

TEST(JoinAligned, AmongOthersTheFirstInInputOrderWinsTheTie) {
    const geom2d::Segment source{{0.0, 0.0}, {10.0, 0.0}};
    const geom2d::Segment a{{12.0, 1e-7}, {20.0, 0.0}};
    const geom2d::Segment b{{20.0, 2e-7}, {15.0, 0.0}};
    expectJoined(geom2d::joinAligned(source, std::array{a, b}), {0.0, 0.0}, {20.0, 0.0});
    expectJoined(geom2d::joinAligned(source, std::array{b, a}), {0.0, 0.0}, {20.0, 2e-7});
}

TEST(JoinAligned, GapsAndExtensionsBeforeTheStartAreIncluded) {
    const geom2d::Segment source{{0.0, 0.0}, {10.0, 0.0}};
    expectJoined(geom2d::joinAligned(source, std::array{geom2d::Segment{{20.0, 0.0}, {30.0, 0.0}}}),
                 {0.0, 0.0}, {30.0, 0.0});
    expectJoined(geom2d::joinAligned(source, std::array{geom2d::Segment{{-5.0, 0.0}, {-1.0, 0.0}}}),
                 {-5.0, 0.0}, {10.0, 0.0});
    expectJoined(geom2d::joinAligned(source, {}), source.start, source.end);
}

TEST(JoinAligned, ReversingTheSourceReversesTheResultAndEndsComeFromTheInputs) {
    Generator generator;
    constexpr std::array<double, 3> kScales{0x1p-40, 1.0, 0x1p20};
    for (int i = 0; i < 400; ++i) {
        const double scale = kScales.at(static_cast<std::size_t>(i) % kScales.size());
        const auto point = [&] {
            return geom2d::Point2{generator.coordinate() * scale, generator.coordinate() * scale};
        };
        const geom2d::Point2 start = point();
        const geom2d::Segment source{start, {start.x + scale, start.y + 0.5 * scale}};
        std::vector<geom2d::Segment> others;
        const auto count = static_cast<std::size_t>(i) % 5U;
        for (std::size_t j = 0; j < count; ++j) {
            // Every third other repeats an end of source, to provoke exact ties.
            const geom2d::Point2 first = j % 3U == 2U ? source.end : point();
            geom2d::Point2 second = point();
            if (samePoint(first, second)) {
                second.x += scale;
            }
            others.push_back({first, second});
        }
        const auto forward = geom2d::joinAligned(source, others);
        const auto backward = geom2d::joinAligned({source.end, source.start}, others);
        ASSERT_TRUE(forward.hasValue());
        expectJoined(backward, forward.value().end, forward.value().start);
        EXPECT_FALSE(samePoint(forward.value().start, forward.value().end));
        const auto isInputEnd = [&](geom2d::Point2 p) {
            bool found = samePoint(p, source.start) || samePoint(p, source.end);
            for (const auto& other : others) {
                found = found || samePoint(p, other.start) || samePoint(p, other.end);
            }
            return found;
        };
        EXPECT_TRUE(isInputEnd(forward.value().start));
        EXPECT_TRUE(isInputEnd(forward.value().end));
    }
}

TEST(JoinAligned, DiagnosesTheFirstBadSegmentInInputOrder) {
    using Code = geom2d::DiagnosisCode;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double far = 0x1p60;
    const geom2d::Segment good{{0.0, 0.0}, {10.0, 0.0}};
    const geom2d::Segment point{{1.0, 1.0}, {1.0, 1.0}};
    const geom2d::Segment invalid{{0.0, 0.0}, {nan, 1.0}};
    const geom2d::Segment outside{{0.0, 0.0}, {far, 0.0}};
    const geom2d::Segment invalidAndOutside{{far, 0.0}, {nan, 0.0}};

    expectDiagnosis(geom2d::joinAligned(point, std::array{invalid}), Code::degenerateCurve, 0);
    expectDiagnosis(geom2d::joinAligned(good, std::array{invalid}), Code::invalidValue, 1);
    expectDiagnosis(geom2d::joinAligned(good, std::array{outside}), Code::outOfDomain, 1);
    expectDiagnosis(geom2d::joinAligned(good, std::array{good, point}), Code::degenerateCurve, 2);
    expectDiagnosis(geom2d::joinAligned(good, std::array{outside, invalid}), Code::outOfDomain, 1);
    expectDiagnosis(geom2d::joinAligned(good, std::array{point, outside}), Code::degenerateCurve,
                    1);
    expectDiagnosis(geom2d::joinAligned(good, std::array{invalidAndOutside}), Code::invalidValue,
                    1);
    expectDiagnosis(geom2d::joinAligned(outside, std::array{point}), Code::outOfDomain, 0);
}

}  // namespace
