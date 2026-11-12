// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "ellipse_meaning.hpp"

#include "positions.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>

namespace {

namespace detail = solid_space::geom2d::detail;
using solid_space::geom2d::Contact;
using solid_space::geom2d::Ellipse;
using solid_space::geom2d::kQuarterTurn;
using solid_space::geom2d::Location;
using solid_space::geom2d::Point2;
using solid_space::robust::Sign;

TEST(EllipseMeaning, ExactImplicitEquationForRotatedAxes) {
    const auto ellipse = detail::checkedEllipse(Ellipse{{2.0, -1.0}, {6.0, 8.0}, 0.5}, 0);
    ASSERT_TRUE(ellipse.hasValue());
    const auto& support = ellipse.value().support;
    const std::array<Point2, 3> vertices{{{8.0, 7.0}, {-2.0, 2.0}, {-4.0, -9.0}}};
    const std::array<Point2, 3> directions{{{1.0, 0.0}, {0.0, 1.0}, {-1.0, 0.0}}};
    for (std::size_t index = 0; index < vertices.size(); ++index) {
        const auto vertex = detail::exactPoint(vertices.at(index));
        const auto mapped =
            detail::ellipsePoint(support, detail::exactVector(directions.at(index)));
        EXPECT_TRUE(detail::samePoint(vertex, mapped));
        EXPECT_EQ(detail::ellipseSign(support, vertex), Sign::zero);
    }
    EXPECT_EQ(detail::ellipseSign(support, detail::exactPoint({2.0, -1.0})), Sign::negative);
    EXPECT_EQ(detail::ellipseSign(support, detail::exactPoint({8.0, 8.0})), Sign::positive);
}

TEST(EllipseMeaning, LocatesExactArcEndpointsAndCarrierPoints) {
    const auto ellipse =
        detail::checkedEllipse(Ellipse{{0.0, 0.0}, {10.0, 0.0}, 0.5, 0.0, 2 * kQuarterTurn}, 0);
    ASSERT_TRUE(ellipse.hasValue());
    const auto& data = ellipse.value();
    const auto end = detail::ellipsePoint(data.support, data.unitArc.endDirection);
    EXPECT_EQ(detail::locateOnEllipse(data, detail::exactPoint({10.0, 0.0})), Location::atStart);
    EXPECT_EQ(detail::locateOnEllipse(data, detail::exactPoint({0.0, 5.0})), Location::inside);
    EXPECT_EQ(detail::locateOnEllipse(data, end), Location::atEnd);
    EXPECT_EQ(detail::locateOnEllipse(data, detail::exactPoint({0.0, -5.0})), Location::afterEnd);
}

TEST(EllipseMeaning, ExactLineMultiplicityAndSegmentPositions) {
    const auto ellipse = detail::checkedEllipse(Ellipse{{0.0, 0.0}, {10.0, 0.0}, 0.5}, 0);
    ASSERT_TRUE(ellipse.hasValue());
    const auto& data = ellipse.value();
    const auto secant = detail::meetEllipseLine(data, {{-10.0, 0.0}, {0.0, 0.0}});
    ASSERT_EQ(secant.count, 2U);
    EXPECT_TRUE(detail::samePoint(secant.points.at(0).point, detail::exactPoint({-10.0, 0.0})));
    EXPECT_EQ(secant.points.at(0).contact, Contact::crossing);
    EXPECT_EQ(secant.points.at(0).onSegment, Location::atStart);
    EXPECT_EQ(secant.points.at(1).onSegment, Location::afterEnd);
    EXPECT_EQ(secant.points.at(1).onEllipse, Location::atStart);

    const auto tangent = detail::meetEllipseLine(data, {{-20.0, 5.0}, {20.0, 5.0}});
    ASSERT_EQ(tangent.count, 1U);
    EXPECT_EQ(tangent.points.at(0).contact, Contact::touching);
    EXPECT_EQ(tangent.points.at(0).onSegment, Location::inside);
    EXPECT_TRUE(detail::samePoint(tangent.points.at(0).point, detail::exactPoint({0.0, 5.0})));

    const auto above = std::nextafter(5.0, std::numeric_limits<double>::infinity());
    const auto below = std::nextafter(5.0, 0.0);
    EXPECT_EQ(detail::meetEllipseLine(data, {{-20.0, above}, {20.0, above}}).count, 0U);
    EXPECT_EQ(detail::meetEllipseLine(data, {{-20.0, below}, {20.0, below}}).count, 2U);
}

TEST(EllipseMeaning, UnitRatioAgreesWithCircleCarrier) {
    const Point2 center{2.0, 1.0};
    const auto ellipse = detail::checkedEllipse(Ellipse{center, {3.0, 4.0}}, 0);
    ASSERT_TRUE(ellipse.hasValue());
    const auto circle = detail::meaningOf(solid_space::geom2d::Circle{center, 5.0});
    for (const double y : {-5.0, -4.0, -1.0, 1.0, 6.0, 7.0}) {
        const detail::LineMeaning line{{-6.0, y}, {12.0, y}};
        const auto actual = detail::meetEllipseLine(ellipse.value(), line);
        const auto expected = detail::meet(line, detail::carrierOf(circle));
        ASSERT_EQ(actual.count, expected.count);
        for (std::size_t index = 0; index < actual.count; ++index) {
            EXPECT_TRUE(
                detail::samePoint(actual.points.at(index).point, expected.points.at(index).point));
            EXPECT_EQ(actual.points.at(index).contact, expected.points.at(index).contact);
        }
    }
}

}  // namespace
