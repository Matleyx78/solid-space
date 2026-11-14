// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "solid_space/geom2d/clipping.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace solid_space::geom2d {
namespace {

Polyline square(double x, double y, double size) {
    return {{{{x, y}, 0}, {{x + size, y}, 0}, {{x + size, y + size}, 0}, {{x, y + size}, 0}}, true};
}

auto bits(double value) {
    return std::bit_cast<std::uint64_t>(value);
}

void expectSameEnd(const StretchEnd& a, const StretchEnd& b) {
    EXPECT_EQ(a.onHost.location, b.onHost.location);
    EXPECT_EQ(bits(a.onHost.station), bits(b.onHost.station));
    EXPECT_EQ(bits(a.point.x), bits(b.point.x));
    EXPECT_EQ(bits(a.point.y), bits(b.point.y));
}

void expectSameHost(const ClippedHost& a, const ClippedHost& b) {
    ASSERT_EQ(a.inside.size(), b.inside.size());
    ASSERT_EQ(a.boundary.size(), b.boundary.size());
    for (std::size_t i = 0; i < a.inside.size(); ++i) {
        expectSameEnd(a.inside.at(i).from, b.inside.at(i).from);
        expectSameEnd(a.inside.at(i).to, b.inside.at(i).to);
    }
    for (std::size_t i = 0; i < a.boundary.size(); ++i) {
        expectSameEnd(a.boundary.at(i).from, b.boundary.at(i).from);
        expectSameEnd(a.boundary.at(i).to, b.boundary.at(i).to);
        EXPECT_EQ(a.boundary.at(i).regionSide, b.boundary.at(i).regionSide);
    }
}

TEST(Clipping, LoopOrderOrientationAndOriginDoNotChangeDecisions) {
    constexpr auto kExtended = Extension::extended;
    const std::array<Loop, 2> loops{square(0, 0, 12), square(4, 4, 4)};
    auto changed = loops;
    for (auto& loop : changed) {
        auto& vertices = std::get<Polyline>(loop).vertices;
        std::ranges::reverse(vertices);
        std::ranges::rotate(vertices, vertices.begin() + 1);
    }
    std::ranges::reverse(changed);
    std::vector<Curve> hosts;
    for (int row = -1; row <= 13; ++row) {
        const double y = row;
        hosts.emplace_back(Segment{{-1, y}, {13, y}});
    }
    const auto a = clipToRegion(hosts, kExtended, loops);
    const auto b = clipToRegion(hosts, kExtended, changed);
    const auto convex = clipToRegion(hosts, kExtended, std::span{loops}.first(1));
    for (std::size_t i = 0; i < hosts.size(); ++i) {
        const auto& clipped = a.hosts.at(i);
        expectSameHost(clipped, b.hosts.at(i));
        expectSameHost(
            clipped, clipToRegion(std::span{hosts}.subspan(i, 1), kExtended, loops).hosts.front());
        EXPECT_LE(convex.hosts.at(i).inside.size(), 1);
        for (const auto& inside : clipped.inside) {
            for (const auto& boundary : clipped.boundary) {
                EXPECT_TRUE(inside.to.onHost.station <= boundary.from.onHost.station ||
                            boundary.to.onHost.station <= inside.from.onHost.station);
            }
        }
    }
}

TEST(Clipping, StartAndReturnHaveDistinctLocationsOnClosedHosts) {
    const std::array<Curve, 2> hosts{Circle{{0, 0}, 1}, Arc{{0, 0}, 1, 0, -kFullTurn}};
    const std::array<Loop, 1> loops{Circle{{0, 0}, 1}};
    const auto result = clipToRegion(hosts, Extension::none, loops);
    for (const auto& host : result.hosts) {
        ASSERT_EQ(host.boundary.size(), 1);
        const auto& stretch = host.boundary.front();
        EXPECT_EQ(stretch.from.onHost.location, Location::atStart);
        EXPECT_EQ(stretch.to.onHost.location, Location::atEnd);
        EXPECT_DOUBLE_EQ(stretch.from.onHost.station, 0.0);
        EXPECT_DOUBLE_EQ(stretch.to.onHost.station, kFullTurn);
    }
    EXPECT_EQ(result.hosts.front().boundary.front().regionSide, Side::left);
    EXPECT_EQ(result.hosts.back().boundary.front().regionSide, Side::right);
}

TEST(Clipping, ShallowArcStationDoesNotRequireARepresentableRadius) {
    constexpr double kBulge = 0x1p-1074;
    const std::array<Curve, 1> hosts{PolylineSegment{{0, 0}, {2, 0}, kBulge}};
    const std::array<Loop, 1> loops{square(-1, -1, 2)};
    const auto result = clipToRegion(hosts, Extension::extended, loops);
    const auto& inside = result.hosts.front().inside;
    ASSERT_EQ(inside.size(), 1);
    EXPECT_EQ(inside.front().from.onHost.location, Location::atStart);
    EXPECT_EQ(inside.front().to.onHost.location, Location::inside);
    EXPECT_NEAR(inside.front().to.onHost.station, 1.0, kRelativeStationError * 2.0);
}

}  // namespace
}  // namespace solid_space::geom2d
