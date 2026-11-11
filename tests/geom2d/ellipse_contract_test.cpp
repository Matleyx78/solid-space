// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// The contract signatures of the ellipse (ADR 0007, point 15; ADR 0017): the
// public header compiles, and the declared functions have the declared
// signatures. Kept apart from contract_test.cpp, which takes the address of
// functions that ellipse.hpp overloads.

#include "solid_space/geom2d/ellipse.hpp"
#include "solid_space/robust/tolerance.hpp"

#include <gtest/gtest.h>

#include <type_traits>
#include <variant>

namespace geom2d = solid_space::geom2d;
namespace robust = solid_space::robust;

namespace {

// The ellipse is not a Curve: the variant of curves.hpp keeps its four
// alternatives.
static_assert(std::variant_size_v<geom2d::Curve> == 4);
static_assert(!std::is_constructible_v<geom2d::Curve, geom2d::Ellipse>);
static_assert(std::is_trivially_copyable_v<geom2d::Ellipse>);

// Signatures of the declared functions, each overload picked by its type and
// checked without odr-using it.
using MagnitudeOfEllipse = geom2d::Result<double>(const geom2d::Ellipse&);
using EndpointsOfEllipse = geom2d::Result<geom2d::Endpoints>(const geom2d::Ellipse&);
using IntersectWithCurve = geom2d::Result<geom2d::EllipseIntersections>(const geom2d::Ellipse&,
                                                                        geom2d::Extension,
                                                                        const geom2d::Curve&,
                                                                        geom2d::Extension);
using IntersectTwoEllipses = geom2d::Result<geom2d::EllipseIntersections>(const geom2d::Ellipse&,
                                                                          geom2d::Extension,
                                                                          const geom2d::Ellipse&,
                                                                          geom2d::Extension);

static_assert(std::is_same_v<decltype(static_cast<MagnitudeOfEllipse*>(&geom2d::magnitude)),
                             MagnitudeOfEllipse*>);
static_assert(std::is_same_v<decltype(static_cast<EndpointsOfEllipse*>(&geom2d::endpoints)),
                             EndpointsOfEllipse*>);
static_assert(std::is_same_v<decltype(static_cast<IntersectWithCurve*>(&geom2d::intersect)),
                             IntersectWithCurve*>);
static_assert(std::is_same_v<decltype(static_cast<IntersectTwoEllipses*>(&geom2d::intersect)),
                             IntersectTwoEllipses*>);
static_assert(
    std::is_same_v<decltype(&geom2d::nearest), geom2d::Result<geom2d::EllipseNearest> (*)(
                                                   const geom2d::Ellipse&, geom2d::Point2)>);
static_assert(std::is_same_v<decltype(&geom2d::meetsRectangle),
                             geom2d::Result<bool> (*)(const geom2d::Ellipse&, geom2d::Point2,
                                                      geom2d::Point2)>);

// The declared error stays well below the relative tolerance (ADR 0009,
// point 5), and above the error of a constructed point.
static_assert(geom2d::kRelativeNearestError == 0x1p-44);
static_assert(4 * geom2d::kRelativeNearestError == robust::kRelativeTolerance);
static_assert(geom2d::kRelativeNearestError > geom2d::kRelativePointError);

}  // namespace

TEST(Geom2dEllipseContract, DefaultEllipseHasTheParametersOfAWholeDxfEllipse) {
    const geom2d::Ellipse ellipse;
    EXPECT_EQ(ellipse.ratio, 1.0);
    EXPECT_EQ(ellipse.startParameter, 0.0);
    EXPECT_EQ(ellipse.endParameter, geom2d::kFullTurn);
}

TEST(Geom2dEllipseContract, ResultsStartWithNothingFound) {
    const geom2d::EllipseIntersections found;
    EXPECT_TRUE(found.points.empty());
    EXPECT_TRUE(found.overlaps.empty());
    EXPECT_EQ(found.omittedBeyondDomain, 0U);
    EXPECT_FALSE(geom2d::EllipseOverlap{}.whole);
    EXPECT_EQ(geom2d::EllipseNearest{}.location, geom2d::Location::inside);
}
