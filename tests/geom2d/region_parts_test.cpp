// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "region_parts.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace solid_space::geom2d {
namespace {

using detail::PartClass;

Loop square(double low, double high) {
    return Polyline{{{{low, low}, 0}, {{high, low}, 0}, {{high, high}, 0}, {{low, high}, 0}},
                    false};
}

TEST(RegionParts, ChecksLoopDataAndWholeCircularSupports) {
    constexpr auto kLimit = robust::kMaxCoordinate;
    const Loop curved = Polyline{{{{0, 0}, 0x1p-60}, {{1, 0}, 0}, {{0, 1}, 0}}, true};
    const std::array loops{Loop{Circle{{0, 0}, -1}}, Loop{Circle{{kLimit, 0}, 1}}, curved,
                           Loop{Circle{{0, 0}, 0}}, Loop{Polyline{{{{0, 0}, 1}}, false}}};
    constexpr std::array kCodes{DiagnosisCode::invalidValue, DiagnosisCode::outOfDomain,
                                DiagnosisCode::outOfDomain, DiagnosisCode::degenerateCurve,
                                DiagnosisCode::degenerateCurve};
    for (std::size_t i = 0; i < loops.size(); ++i) {
        const auto result = detail::checkedLoopPieces(loops.at(i), i);
        ASSERT_FALSE(result.hasValue());
        EXPECT_EQ(result.diagnosis().code, kCodes.at(i));
        EXPECT_EQ(result.diagnosis().input, i);
    }
    EXPECT_TRUE(detail::checkedLoopPieces(Circle{{kLimit / 2, 0}, kLimit / 2}, 0).hasValue());
}

TEST(RegionParts, DropsPointPiecesAndClosesTheLoop) {
    const Loop loop =
        Polyline{{{{0, 0}, 0}, {{0, 0}, 0}, {{2, 0}, 0}, {{2, 2}, 0}, {{0, 2}, 0}}, false};
    const auto result = detail::checkedLoopPieces(loop, 0);
    ASSERT_TRUE(result.hasValue());
    ASSERT_EQ(result.value().pieces.size(), 4);
    EXPECT_EQ(result.value().pieces.front().index, 1);
    EXPECT_EQ(result.value().pieces.back().index, 4);
}

TEST(RegionParts, ClassesAtVerticesTangenciesAndSharedEdges) {
    const auto box = detail::checkedLoopPieces(square(0, 4), 0).value();
    const auto island = detail::checkedLoopPieces(Circle{{2, 2}, 1}, 1).value();
    const auto diagonal = detail::meaningOf(Segment{{-1, -1}, {5, 5}});
    const auto tangent = detail::meaningOf(Segment{{-1, 3}, {5, 3}});
    const auto edge = detail::meaningOf(Segment{{-1, 0}, {5, 0}});
    constexpr auto kNone = Extension::none;
    EXPECT_EQ(detail::regionParts(diagonal, kNone, std::array{box}).classes,
              (std::vector{PartClass::outside, PartClass::inside, PartClass::outside}));
    EXPECT_EQ(detail::regionParts(tangent, kNone, std::array{box, island}).classes,
              (std::vector{PartClass::outside, PartClass::inside, PartClass::inside,
                           PartClass::outside}));
    EXPECT_EQ(detail::regionParts(edge, kNone, std::array{box}).classes,
              (std::vector{PartClass::outside, PartClass::boundaryLeft, PartClass::outside}));
    EXPECT_EQ(detail::regionParts(edge, kNone, std::array{box, box}).classes,
              (std::vector{PartClass::outside, PartClass::outside, PartClass::outside}));
}

// --- the index of the region (package FS1 of the 0.3 performance work) --------

using detail::PieceSearch;
using detail::RegionIndex;

// The bulge of the second side of every edge of the square below.
double sideBulge(std::size_t edge) {
    return edge % 2 == 0 ? 0.25 : -0.5;
}

// An outer square with many sides, three of them curved, with islands: two
// circles, a curved polygon, a rectangle and a triangle with a slanted side.
std::vector<detail::LoopPieces> islands() {
    Polyline outer{{}, true};
    for (std::size_t edge = 0; edge < 4; ++edge) {
        for (std::size_t k = 0; k < 4; ++k) {
            const double step = 4.0 * static_cast<double>(k);
            const std::array<Point2, 4> points{
                {{step, 0}, {16, step}, {16 - step, 16}, {0, 16 - step}}};
            const double bulge = k == 1 && edge != 3 ? sideBulge(edge) : 0.0;
            outer.vertices.push_back({points.at(edge), bulge});
        }
    }
    const std::array<Loop, 6> loops{
        outer,
        Circle{{4, 4}, 2},
        Circle{{12, 4}, 1.5},
        Polyline{{{{2, 9}, 0.5}, {{6, 9}, 0}, {{6, 14}, -1.5}, {{2, 14}, 0}}, true},
        square(8, 12),
        Polyline{{{{13, 13}, 0}, {{15, 13}, 0}, {{13, 15}, 0}}, true}};
    std::vector<detail::LoopPieces> result;
    for (std::size_t i = 0; i < loops.size(); ++i) {
        result.push_back(detail::checkedLoopPieces(loops.at(i), i).value());
    }
    return result;
}

void expectSameParts(const detail::RegionParts& actual, const detail::RegionParts& expected,
                     std::size_t host) {
    EXPECT_EQ(actual.closed, expected.closed) << "host " << host;
    EXPECT_EQ(actual.classes, expected.classes) << "host " << host;
    EXPECT_EQ(actual.onLoop, expected.onLoop) << "host " << host;
    ASSERT_EQ(actual.cuts.size(), expected.cuts.size()) << "host " << host;
    for (std::size_t i = 0; i < actual.cuts.size(); ++i) {
        EXPECT_TRUE(detail::samePoint(actual.cuts.at(i), expected.cuts.at(i)))
            << "host " << host << ", cut " << i;
    }
}

// The parts of every host with one index for all of them, as clipToRegion
// does, and with a new index for each host, against the parts of the exact
// path that looks at every piece.
void expectSameWithIndex(const std::vector<Curve>& hosts,
                         std::span<const detail::LoopPieces> loops) {
    RegionIndex boxes(loops, PieceSearch::boxes);
    RegionIndex every(loops, PieceSearch::everyPiece);
    for (std::size_t i = 0; i < hosts.size(); ++i) {
        const auto meaning = detail::meaningOf(hosts.at(i));
        for (const auto extension : {Extension::none, Extension::extended}) {
            const auto expected = detail::regionParts(meaning, extension, every);
            expectSameParts(detail::regionParts(meaning, extension, boxes), expected, i);
            expectSameParts(detail::regionParts(meaning, extension, loops), expected, i);
        }
    }
}

// Lines through the region in the direction (dx, dy), one for each offset
// along the normal, from (x, y) shifted by offset * (-dy, dx).
std::vector<Curve> rows(Point2 from, Point2 direction, double first, double last, double step) {
    std::vector<Curve> result;
    const auto count = static_cast<std::size_t>((last - first) / step);
    for (std::size_t k = 0; k <= count; ++k) {
        const double offset = first + (static_cast<double>(k) * step);
        const Point2 start{from.x - (offset * direction.y), from.y + (offset * direction.x)};
        result.emplace_back(
            Segment{start, {start.x + (20 * direction.x), start.y + (20 * direction.y)}});
    }
    return result;
}

TEST(RegionParts, ParallelRowsGiveThePartsOfEveryPiece) {
    const auto loops = islands();
    // Horizontal and vertical rows every half unit: through vertices, at the
    // heights of the tops of circles and arcs, along the sides of the square
    // and of the rectangle.
    expectSameWithIndex(rows({-2, 0}, {1, 0}, -1, 17, 0.5), loops);
    expectSameWithIndex(rows({0, -2}, {0, 1}, -17, 1, 0.5), loops);
    // A row that turns by 2^-30 from the horizontal.
    expectSameWithIndex(rows({-2, 0}, {1, 0x1p-30}, -1, 17, 1), loops);
    // Rows that turn by about 2^-21 from the first, horizontal one, so that
    // they use its sorting: each starts below a side that it meets further on
    // (the sides x = 8 and x = 12 of the rectangle start at y = 8, the
    // bottom of the triangle is at y = 13).
    std::vector<Curve> tilted{Segment{{-2, 0}, {18, 0}}};
    for (const double y : {8.0, 13.0, 4.0, 16.0}) {
        tilted.emplace_back(Segment{{-2, y - 0x1p-19}, {18, y - 0x1p-19 + (20 * 0x1p-21)}});
        tilted.emplace_back(Segment{{18, y - 0x1p-19}, {-2, y - 0x1p-19 - (20 * 0x1p-21)}});
    }
    expectSameWithIndex(tilted, loops);
}

TEST(RegionParts, ObliqueRowsGiveThePartsOfEveryPiece) {
    const auto loops = islands();
    // Diagonals through the vertices, and the slanted side of the triangle
    // with the line that carries it.
    expectSameWithIndex(rows({-2, -2}, {1, 1}, -12, 12, 0.5), loops);
    expectSameWithIndex(rows({16, -2}, {-1, 1}, -12, 12, 0.5), loops);
    expectSameWithIndex(rows({-2, -1}, {3, 1}, -4, 4, 0.25), loops);
    expectSameWithIndex(rows({2, 18}, {1, -2}, -8, 8, 0.25), loops);
    expectSameWithIndex({Segment{{15, 13}, {13, 15}}, Segment{{16, 12}, {12, 16}},
                         Segment{{13, 13}, {15, 13}}, Segment{{8, 8}, {8, 12}}},
                        loops);
}

TEST(RegionParts, MixedHostsGiveThePartsOfEveryPiece) {
    // Directions that change from one host to the next sort the boxes again;
    // circles, arcs and hosts too short for the keys look at the boxes only.
    const auto loops = islands();
    std::vector<Curve> hosts;
    const auto horizontal = rows({-2, 0}, {1, 0}, 0, 16, 2);
    const auto vertical = rows({0, -2}, {0, 1}, -16, 0, 2);
    const auto diagonal = rows({-2, -2}, {1, 1}, -8, 8, 2);
    for (std::size_t i = 0; i < horizontal.size(); ++i) {
        hosts.push_back(horizontal.at(i));
        hosts.push_back(vertical.at(i));
        hosts.push_back(diagonal.at(i));
    }
    hosts.emplace_back(Circle{{8, 8}, 5});
    hosts.emplace_back(Circle{{4, 4}, 2});
    hosts.emplace_back(Circle{{12, 4}, 3});
    hosts.emplace_back(Arc{{8, 8}, 6, 0.5, 2.0});
    hosts.emplace_back(PolylineSegment{{2, 9}, {6, 9}, 0.5});
    hosts.emplace_back(PolylineSegment{{-1, 3}, {17, 5}, -0.125});
    // Near zero, so that the ends differ in the doubles.
    hosts.emplace_back(Segment{{0, 5}, {0x1p-520, 5}});
    hosts.emplace_back(Segment{{0, 0}, {0x1p-600, 0x1p-600}});
    hosts.emplace_back(Segment{{4, 4}, {4, 4 + 0x1p-40}});
    expectSameWithIndex(hosts, loops);
}

}  // namespace
}  // namespace solid_space::geom2d
