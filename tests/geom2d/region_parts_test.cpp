// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "region_parts.hpp"

#include <gtest/gtest.h>

#include <array>
#include <limits>
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
    auto loop = std::get<Polyline>(square(0, 2));
    loop.vertices.insert(loop.vertices.begin(), loop.vertices.front());
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

}  // namespace
}  // namespace solid_space::geom2d
