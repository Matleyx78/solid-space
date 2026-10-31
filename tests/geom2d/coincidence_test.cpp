// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "solid_space/geom2d/coincidence.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

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
        const std::array<geom2d::Curve, 3> curves{
            geom2d::Circle{center, radius},
            geom2d::Arc{center, radius, 0.25, -2.0},
            geom2d::PolylineSegment{{center.x - radius, center.y},
                                    {center.x + radius, center.y}, 1.0}};
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
        const geom2d::PolylineSegment piece{{center.x, center.y},
                                            {center.x + 1.0, center.y + 2.0}, 0.3};
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

}  // namespace
