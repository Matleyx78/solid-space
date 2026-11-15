// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "solid_space/geom2d/boundary.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <numeric>
#include <vector>

namespace solid_space::geom2d {
namespace {

Polyline square(double x, double y, double size) {
    return {{{{x, y}, 0}, {{x + size, y}, 0}, {{x + size, y + size}, 0}, {{x, y + size}, 0}}, true};
}

Polyline squareWithSpur(double size, double spurX) {
    auto loop = square(0, 0, size);
    loop.vertices.insert(loop.vertices.begin(), {{{0, 0}, 0}, {{spurX, 0}, 0}});
    return loop;
}

TEST(NestContours, DoubleTraversalHasTheSameTrace) {
    const auto once = square(0, 0, 4);
    auto twice = once;
    twice.vertices.insert(twice.vertices.end(), once.vertices.begin(), once.vertices.end());
    const std::array<Loop, 2> contours{once, twice};

    const auto result = nestContours(contours);

    ASSERT_EQ(result.depth.size(), contours.size());
    EXPECT_EQ(result.depth.at(0), 0U);
    EXPECT_EQ(result.depth.at(1), 0U);
    ASSERT_EQ(result.conflicts.size(), 1U);
    const auto& conflict = result.conflicts.front();
    EXPECT_EQ(conflict.first, 0U);
    EXPECT_EQ(conflict.second, 1U);
    EXPECT_EQ(conflict.kind, ContourConflictKind::same);
    EXPECT_TRUE(result.skipped.empty());
}

TEST(NestContours, RetracedSpurIsOnTheLoopEvenWhenBothSidesAreOutside) {
    const std::array<Loop, 2> contours{squareWithSpur(4, -2), squareWithSpur(2, -1)};

    const auto result = nestContours(contours);

    ASSERT_EQ(result.depth.size(), contours.size());
    EXPECT_EQ(result.depth.at(0), 0U);
    EXPECT_EQ(result.depth.at(1), 1U);
    EXPECT_TRUE(result.conflicts.empty());
    EXPECT_TRUE(result.skipped.empty());
}

TEST(NestContours, PermutationsAndPolylineStartAndDirectionPreserveNesting) {
    constexpr std::size_t kLevels = 4;
    std::vector<Loop> contours;
    for (std::size_t i = 0; i < kLevels; ++i) {
        const auto inset = static_cast<double>(i);
        contours.emplace_back(square(inset, inset, 10 - 2 * inset));
    }
    // These two contours cross each other and are disjoint from the chain.
    contours.emplace_back(square(20, 0, 4));
    contours.emplace_back(square(22, 2, 4));

    std::vector<std::size_t> order(contours.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    do {
        std::vector<Loop> permuted;
        for (const auto index : order) {
            auto loop = std::get<Polyline>(contours.at(index));
            std::ranges::reverse(loop.vertices);
            std::ranges::rotate(loop.vertices, loop.vertices.begin() + 1);
            permuted.emplace_back(loop);
        }

        const auto result = nestContours(permuted);

        ASSERT_EQ(result.depth.size(), order.size());
        for (std::size_t i = 0; i < order.size(); ++i) {
            const auto index = order.at(i);
            // Each smaller square in the chain has one encloser per preceding level.
            EXPECT_EQ(result.depth.at(i), index < kLevels ? index : 0U);
        }
        ASSERT_EQ(result.conflicts.size(), 1U);
        const auto& conflict = result.conflicts.front();
        EXPECT_LT(conflict.first, conflict.second);
        EXPECT_GE(order.at(conflict.first), kLevels);
        EXPECT_GE(order.at(conflict.second), kLevels);
        EXPECT_EQ(conflict.kind, ContourConflictKind::crossing);
        EXPECT_TRUE(result.skipped.empty());
    } while (std::next_permutation(order.begin(), order.end()));
}

}  // namespace
}  // namespace solid_space::geom2d
