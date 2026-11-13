// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "solid_space/geom2d/ellipse.hpp"

#include "ellipse_meaning.hpp"
#include "positions.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>

namespace {

namespace geom2d = solid_space::geom2d;
namespace detail = solid_space::geom2d::detail;
using geom2d::Ellipse;
using geom2d::Extension;
using geom2d::Point2;
using solid_space::robust::Sign;

TEST(Ellipse, WholeEllipseEndpointsMatchBitForBitOutsideDomain) {
    const double limit = solid_space::robust::kMaxCoordinate;
    const auto result = geom2d::endpoints(Ellipse{{limit, -0.0}, {limit, 0.0}});
    ASSERT_TRUE(result.hasValue());
    const auto& [start, end] = result.value();
    EXPECT_DOUBLE_EQ(start.x, 2 * limit);
    EXPECT_EQ(std::bit_cast<std::uint64_t>(start.x), std::bit_cast<std::uint64_t>(end.x));
    EXPECT_EQ(std::bit_cast<std::uint64_t>(start.y), std::bit_cast<std::uint64_t>(end.y));
}

TEST(Ellipse, RectangleAgreesWithExactStartAndSidesInBothCornerOrders) {
    for (const double start : {-1.0, 0.0, 1.0, 2.0}) {
        const Ellipse ellipse{{1.0, -1.0}, {3.0, 4.0}, 0.5, start, -0.5};
        const auto meaning = detail::checkedEllipse(ellipse, 0);
        ASSERT_TRUE(meaning.hasValue());
        const auto& data = meaning.value();
        const auto exactStart = detail::ellipsePoint(data.support, data.unitArc.startDirection);
        for (int x = -5; x <= 5; ++x) {
            for (int y = -5; y <= 5; ++y) {
                const Point2 low{static_cast<double>(x), static_cast<double>(y)};
                const Point2 high{low.x + 2.0, low.y + 1.0};
                const auto minimum = detail::exactPoint(low);
                const auto maximum = detail::exactPoint(high);
                const bool startInside = detail::compareX(exactStart, minimum) != Sign::negative &&
                                         detail::compareX(exactStart, maximum) != Sign::positive &&
                                         detail::compareY(exactStart, minimum) != Sign::negative &&
                                         detail::compareY(exactStart, maximum) != Sign::positive;
                const Point2 upperLeft{low.x, high.y};
                const Point2 lowerRight{high.x, low.y};
                const std::array<detail::LineMeaning, 4> sides{
                    {{low, upperLeft}, {upperLeft, high}, {high, lowerRight}, {lowerRight, low}}};
                const bool meetsSide = std::ranges::any_of(sides, [&](const auto& side) {
                    const auto hits = detail::meetEllipseLine(data, side);
                    return std::ranges::any_of(hits.found(), [](const auto& hit) {
                        return detail::takesPart(hit.onEllipse, Extension::none) &&
                               detail::takesPart(hit.onSegment, Extension::none);
                    });
                });
                const auto direct = geom2d::meetsRectangle(ellipse, low, high);
                const auto swapped = geom2d::meetsRectangle(ellipse, high, low);
                ASSERT_TRUE(direct.hasValue());
                ASSERT_TRUE(swapped.hasValue());
                EXPECT_EQ(direct.value(), startInside || meetsSide);
                EXPECT_EQ(swapped.value(), direct.value());
            }
        }
    }
}

}  // namespace
