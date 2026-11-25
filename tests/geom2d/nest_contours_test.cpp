// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "region_parts.hpp"
#include "solid_space/geom2d/boundary.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>
#include <string>
#include <utility>
#include <variant>
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
    bool more = true;
    while (more) {
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
        more = std::next_permutation(order.begin(), order.end());
    }
}

// The package FS5 of the 0.3 performance work: the boxes and the parity of
// one point give the answers of the exact path of stage 0.3 (every pair
// through regionParts()), on the same contours.
void expectSameAsEveryPair(std::span<const Loop> contours, const std::string& name) {
    const auto fast = detail::nestContours(contours, detail::NestSearch::boxes);
    const auto exact = detail::nestContours(contours, detail::NestSearch::everyPair);
    const auto publicResult = nestContours(contours);
    EXPECT_EQ(fast.depth, exact.depth) << name;
    EXPECT_EQ(publicResult.depth, exact.depth) << name;
    ASSERT_EQ(fast.conflicts.size(), exact.conflicts.size()) << name;
    for (std::size_t i = 0; i < exact.conflicts.size(); ++i) {
        EXPECT_EQ(fast.conflicts.at(i).first, exact.conflicts.at(i).first) << name << ' ' << i;
        EXPECT_EQ(fast.conflicts.at(i).second, exact.conflicts.at(i).second) << name << ' ' << i;
        EXPECT_EQ(fast.conflicts.at(i).kind, exact.conflicts.at(i).kind) << name << ' ' << i;
    }
    ASSERT_EQ(fast.skipped.size(), exact.skipped.size()) << name;
    for (std::size_t i = 0; i < exact.skipped.size(); ++i) {
        EXPECT_EQ(fast.skipped.at(i).index, exact.skipped.at(i).index) << name << ' ' << i;
        EXPECT_EQ(fast.skipped.at(i).diagnosis.code, exact.skipped.at(i).diagnosis.code)
            << name << ' ' << i;
    }
}

Polyline rectangle(double x, double y, double width, double height, double bulge = 0) {
    return {
        {{{x, y}, 0}, {{x + width, y}, bulge}, {{x + width, y + height}, 0}, {{x, y + height}, 0}},
        true};
}

TEST(NestContours, BoxesMatchEveryPairOnChosenRelations) {
    const Polyline triangle{{{{0, 0}, 0}, {{4, 0}, 0}, {{0, 3}, 0}}, true};
    auto reversed = rectangle(0, 0, 4, 4);
    std::ranges::reverse(reversed.vertices);
    const std::vector<std::pair<std::string, std::vector<Loop>>> families{
        {"disjoint", {square(0, 0, 1), square(3, 0, 1), Circle{{10, 10}, 1}}},
        {"boxes overlap, apart", {triangle, Circle{{3, 3}, 1}}},
        {"nested", {square(0, 0, 10), square(2, 2, 2), Circle{{5, 5}, 4}, Circle{{3, 3}, 0.5}}},
        {"concentric", {Circle{{0, 0}, 1}, Circle{{0, 0}, 2}, Circle{{0, 0}, 3}}},
        {"tangent inside", {Circle{{0, 0}, 4}, Circle{{1, 0}, 3}, square(-2, -2, 2)}},
        {"tangent outside", {Circle{{0, 0}, 1}, Circle{{3, 0}, 2}, square(1, -1, 2)}},
        {"corner touching", {square(0, 0, 2), square(2, 2, 2), square(0, 0, 4)}},
        {"equal", {square(0, 0, 4), reversed, Circle{{0, 0}, 2}, Circle{{0, 0}, 2}}},
        {"crossing", {square(0, 0, 4), square(2, 2, 4), Circle{{0, 0}, 2}}},
        {"common side", {square(0, 0, 2), square(2, 0, 2), rectangle(0, 0, 4, 2)}},
        {"bulged sides",
         {rectangle(0, 0, 4, 2, 1), rectangle(0, 0, 4, 2, -1), Circle{{4, 1}, 1}, square(1, 0, 1)}},
        {"skipped", {Circle{{0, 0}, 0}, square(0, 0, 2), Circle{{1, 1}, 0.5}}},
    };
    for (const auto& [name, contours] : families) {
        expectSameAsEveryPair(contours, name);
    }
}

// A linear congruential generator with a fixed seed: the same contours on
// every system.
class Generator {
public:
    std::uint32_t next() {
        m_state = (m_state * 1664525U) + 1013904223U;
        return m_state >> 8U;
    }

    // A multiple of 1/2 in [0, half * steps).
    double grid(std::uint32_t steps) { return static_cast<double>(next() % steps) / 2; }

private:
    std::uint32_t m_state = 0x5f5U;
};

Loop randomContour(Generator& generator, std::span<const Loop> earlier) {
    constexpr std::array kBulges{0.0, 0.0, 1.0, -1.0, 0.5};
    const auto kind = generator.next() % 6;
    if (kind == 0 && !earlier.empty()) {
        // The same set of points again, reversed for a polyline.
        auto copy = earlier[generator.next() % earlier.size()];
        if (auto* polyline = std::get_if<Polyline>(&copy)) {
            std::ranges::reverse(polyline->vertices);
        }
        return copy;
    }
    const double x = generator.grid(12);
    const double y = generator.grid(12);
    if (kind <= 2) {
        return Circle{{x, y}, generator.grid(8)};
    }
    const double width = 1 + generator.grid(8);
    const double height = 1 + generator.grid(8);
    if (kind == 3) {
        return Polyline{{{{x, y}, 0}, {{x + width, y}, 0}, {{x, y + height}, 0}}, true};
    }
    return rectangle(x, y, width, height, kBulges.at(generator.next() % kBulges.size()));
}

TEST(NestContours, BoxesMatchEveryPairOnSeededContours) {
    constexpr std::size_t kTrials = 150;
    constexpr std::size_t kContours = 10;
    Generator generator;
    std::size_t same = 0;
    std::size_t crossing = 0;
    std::size_t enclosed = 0;
    for (std::size_t trial = 0; trial < kTrials; ++trial) {
        std::vector<Loop> contours;
        for (std::size_t i = 0; i < kContours; ++i) {
            contours.push_back(randomContour(generator, contours));
        }
        expectSameAsEveryPair(contours, "trial " + std::to_string(trial));
        const auto result = nestContours(contours);
        for (const auto& conflict : result.conflicts) {
            ++(conflict.kind == ContourConflictKind::same ? same : crossing);
        }
        for (const auto& depth : result.depth) {
            enclosed += static_cast<std::size_t>(depth.value_or(0) > 0);
        }
    }
    // The seed gives every relation, so the comparison above covers them.
    EXPECT_GT(same, 0U);
    EXPECT_GT(crossing, 0U);
    EXPECT_GT(enclosed, 0U);
}

}  // namespace
}  // namespace solid_space::geom2d
