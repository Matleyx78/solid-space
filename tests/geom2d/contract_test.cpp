// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// The contract signatures of geom2d (ADR 0007, point 15; ADR 0009 to ADR 0012,
// ADR 0014, ADR 0015):
// the public headers compile, and the declared functions have the declared
// signatures.

#include "solid_space/geom2d/boundary.hpp"
#include "solid_space/geom2d/clipping.hpp"
#include "solid_space/geom2d/coincidence.hpp"
#include "solid_space/geom2d/curves.hpp"
#include "solid_space/geom2d/diagnosis.hpp"
#include "solid_space/geom2d/intersection.hpp"
#include "solid_space/geom2d/offset.hpp"
#include "solid_space/geom2d/region.hpp"
#include "solid_space/geom2d/side.hpp"
#include "solid_space/geom2d/tangency.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <type_traits>
#include <variant>
#include <vector>

namespace geom2d = solid_space::geom2d;
namespace robust = solid_space::robust;

namespace {

// The point type is the one of robust: no conversion between the two.
static_assert(std::is_same_v<geom2d::Point2, robust::Point2>);

// The four curves, in the order of the variant.
static_assert(std::variant_size_v<geom2d::Curve> == 4);
static_assert(std::is_same_v<std::variant_alternative_t<0, geom2d::Curve>, geom2d::Segment>);
static_assert(std::is_same_v<std::variant_alternative_t<1, geom2d::Curve>, geom2d::Arc>);
static_assert(std::is_same_v<std::variant_alternative_t<2, geom2d::Curve>, geom2d::Circle>);
static_assert(
    std::is_same_v<std::variant_alternative_t<3, geom2d::Curve>, geom2d::PolylineSegment>);
static_assert(std::is_trivially_copyable_v<geom2d::Curve>);

// Signatures of the declared functions, checked without odr-using them.
static_assert(std::is_same_v<decltype(&geom2d::pieces),
                             std::vector<geom2d::PolylineSegment> (*)(const geom2d::Polyline&)>);
static_assert(
    std::is_same_v<decltype(&geom2d::magnitude), geom2d::Result<double> (*)(const geom2d::Curve&)>);
static_assert(std::is_same_v<decltype(&geom2d::endpoints),
                             geom2d::Result<geom2d::Endpoints> (*)(const geom2d::Curve&)>);
static_assert(
    std::is_same_v<decltype(&geom2d::length), geom2d::Result<double> (*)(const geom2d::Curve&)>);
static_assert(
    std::is_same_v<decltype(&geom2d::intersect), geom2d::Result<geom2d::PairIntersections> (*)(
                                                     const geom2d::Curve&, geom2d::Extension,
                                                     const geom2d::Curve&, geom2d::Extension)>);
static_assert(std::is_same_v<decltype(&geom2d::intersectAlong),
                             geom2d::Result<geom2d::HostIntersections> (*)(
                                 const geom2d::Curve&, geom2d::Extension,
                                 std::span<const geom2d::Curve>, geom2d::Extension)>);
static_assert(std::is_same_v<decltype(&geom2d::side), geom2d::Result<geom2d::Side> (*)(
                                                          const geom2d::Curve&, geom2d::Point2)>);
static_assert(
    std::is_same_v<decltype(&geom2d::isDegenerate),
                   geom2d::Result<bool> (*)(const geom2d::Curve&, const robust::TolerancePolicy&)>);
static_assert(std::is_same_v<decltype(&geom2d::isStraight),
                             geom2d::Result<bool> (*)(const geom2d::PolylineSegment&,
                                                      const robust::TolerancePolicy&)>);
static_assert(
    std::is_same_v<decltype(&geom2d::areAligned),
                   geom2d::Result<bool> (*)(const geom2d::Segment&, const geom2d::Segment&,
                                            const robust::TolerancePolicy&)>);
static_assert(std::is_same_v<decltype(&geom2d::areConcentric),
                             geom2d::Result<bool> (*)(const geom2d::Curve&, const geom2d::Curve&,
                                                      const robust::TolerancePolicy&)>);
static_assert(std::is_same_v<decltype(&geom2d::onSameCircle),
                             geom2d::Result<bool> (*)(const geom2d::Curve&, const geom2d::Curve&,
                                                      const robust::TolerancePolicy&)>);
static_assert(std::is_same_v<decltype(&geom2d::joinAligned),
                             geom2d::Result<geom2d::Segment> (*)(
                                 const geom2d::Segment&, std::span<const geom2d::Segment>)>);

// Tangency (ADR 0011).
static_assert(std::is_same_v<decltype(&geom2d::tangentsFrom),
                             geom2d::Result<geom2d::TangentsFromPoint> (*)(
                                 geom2d::Point2, const geom2d::Curve&, geom2d::Extension)>);
static_assert(std::is_same_v<decltype(&geom2d::tangentCircles),
                             geom2d::Result<geom2d::TangentCircles> (*)(
                                 const geom2d::Curve&, const geom2d::Curve&, double)>);
static_assert(std::is_same_v<decltype(&geom2d::tangentCircleNear),
                             geom2d::Result<geom2d::ChosenCircle> (*)(
                                 const geom2d::Curve&, geom2d::Point2, const geom2d::Curve&,
                                 geom2d::Point2, double)>);
static_assert(
    std::is_same_v<decltype(&geom2d::fillet), geom2d::Result<geom2d::Fillet> (*)(
                                                  const geom2d::Curve&, geom2d::Point2,
                                                  const geom2d::Curve&, geom2d::Point2, double)>);

// The part of each curve a fillet keeps (decision #184): two values in one
// byte, and both fields of Fillet hold one.
static_assert(std::is_same_v<std::underlying_type_t<geom2d::KeptPart>, std::uint8_t>);
static_assert(static_cast<int>(geom2d::KeptPart::fromStart) == 0);
static_assert(static_cast<int>(geom2d::KeptPart::toEnd) == 1);
static_assert(std::is_same_v<decltype(geom2d::Fillet::keepFirst), geom2d::KeptPart>);
static_assert(std::is_same_v<decltype(geom2d::Fillet::keepSecond), geom2d::KeptPart>);

// Offset (ADR 0012).
static_assert(
    std::is_same_v<decltype(&geom2d::offsetCurve),
                   geom2d::Result<geom2d::CurveOffset> (*)(const geom2d::Curve&, double)>);
static_assert(std::is_same_v<decltype(&geom2d::offsetPolyline),
                             geom2d::Result<geom2d::PolylineOffset> (*)(
                                 const geom2d::Polyline&, double, const robust::TolerancePolicy&)>);

// Boundaries and regions (ADR 0014).
static_assert(std::variant_size_v<geom2d::Loop> == 2);
static_assert(std::is_same_v<std::variant_alternative_t<0, geom2d::Loop>, geom2d::Circle>);
static_assert(std::is_same_v<std::variant_alternative_t<1, geom2d::Loop>, geom2d::Polyline>);
static_assert(std::is_same_v<decltype(&geom2d::detectBoundary),
                             geom2d::Result<geom2d::DetectedBoundary> (*)(
                                 geom2d::Point2, std::span<const geom2d::Curve>,
                                 const robust::TolerancePolicy&)>);
static_assert(std::is_same_v<decltype(&geom2d::nestContours),
                             geom2d::ContourNesting (*)(std::span<const geom2d::Loop>)>);
static_assert(std::is_same_v<std::underlying_type_t<geom2d::BoundaryOutcome>, std::uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<geom2d::LoopPieceSource>, std::uint8_t>);
static_assert(std::is_same_v<std::underlying_type_t<geom2d::ContourConflictKind>, std::uint8_t>);

// Curves cut on a region (ADR 0015).
static_assert(
    std::is_same_v<decltype(&geom2d::clipToRegion),
                   geom2d::RegionClipping (*)(std::span<const geom2d::Curve>, geom2d::Extension,
                                              std::span<const geom2d::Loop>)>);
static_assert(std::is_same_v<decltype(geom2d::StretchEnd{}.onHost), geom2d::CurvePosition>);
static_assert(std::is_same_v<decltype(geom2d::BoundaryStretch{}.regionSide), geom2d::Side>);

// The constants of the contract: the angle limits are the doubles nearest to
// pi / 2, 3 pi / 2 and 2 pi, and the construction errors stay well below the
// relative tolerance (ADR 0009, point 5).
static_assert(geom2d::kQuarterTurn == 1.5707963267948966);
static_assert(geom2d::kThreeQuarterTurn == 4.71238898038469);
static_assert(geom2d::kFullTurn == 6.283185307179586);
static_assert(geom2d::kRelativePointError == 0x1p-48);
static_assert(geom2d::kRelativeStationError == 0x1p-46);
static_assert(64 * geom2d::kRelativePointError == robust::kRelativeTolerance);
static_assert(16 * geom2d::kRelativeStationError == robust::kRelativeTolerance);
static_assert(geom2d::kRelativeArcError == 0x1p-46);
static_assert(geom2d::kRelativeOffsetError == 0x1p-46);
static_assert(16 * geom2d::kRelativeArcError == robust::kRelativeTolerance);
static_assert(16 * geom2d::kRelativeOffsetError == robust::kRelativeTolerance);
static_assert(geom2d::kRelativeLoopError == 0x1p-44);
static_assert(4 * geom2d::kRelativeLoopError == robust::kRelativeTolerance);

}  // namespace

TEST(Geom2dContract, ResultCarriesTheInputOfTheDiagnosis) {
    const geom2d::Result<bool> result(
        geom2d::Diagnosis{geom2d::DiagnosisCode::degenerateCurve, 1, "segment is a point"});
    ASSERT_FALSE(result.hasValue());
    EXPECT_EQ(result.diagnosis().code, geom2d::DiagnosisCode::degenerateCurve);
    EXPECT_EQ(result.diagnosis().input, 1U);
}

TEST(Geom2dContract, CurvesAreAggregatesOfTheirData) {
    const geom2d::Curve curve = geom2d::PolylineSegment{{0.0, 0.0}, {2.0, 0.0}, 1.0};
    ASSERT_TRUE(std::holds_alternative<geom2d::PolylineSegment>(curve));
    EXPECT_EQ(std::get<geom2d::PolylineSegment>(curve).bulge, 1.0);
}

TEST(Geom2dContract, ChoicesStartWithNothingFound) {
    // A default result never claims a construction that was not made.
    EXPECT_EQ(geom2d::ChosenCircle{}.outcome, geom2d::TangencyOutcome::none);
    EXPECT_EQ(geom2d::Fillet{}.outcome, geom2d::TangencyOutcome::none);
    EXPECT_EQ(geom2d::Fillet{}.keepFirst, geom2d::KeptPart::fromStart);
    EXPECT_EQ(geom2d::Fillet{}.keepSecond, geom2d::KeptPart::fromStart);
    EXPECT_EQ(geom2d::PolylineOffset{}.outcome, geom2d::OffsetOutcome::failed);
    EXPECT_EQ(geom2d::PolylineOffset{}.failure, geom2d::OffsetFailure::none);
    EXPECT_FALSE(geom2d::TangentCircles{}.infinitelyMany);
}

TEST(Geom2dContract, BoundaryResultsStartWithNothingFound) {
    // A default detection encloses nothing and lists nothing.
    const geom2d::DetectedBoundary boundary;
    EXPECT_EQ(boundary.outcome, geom2d::BoundaryOutcome::notEnclosed);
    EXPECT_TRUE(boundary.loops.empty());
    EXPECT_TRUE(boundary.openEnds.empty());
    EXPECT_TRUE(boundary.skipped.empty());
    EXPECT_EQ(geom2d::RegionLoop{}.depth, 0U);
    EXPECT_EQ(geom2d::LoopPieceOrigin{}.source, geom2d::LoopPieceSource::curve);
    EXPECT_TRUE(geom2d::ContourNesting{}.depth.empty());
}

TEST(Geom2dContract, LoopsAreCirclesOrPolylines) {
    const geom2d::Loop loop = geom2d::Circle{{1.0, 2.0}, 3.0};
    ASSERT_TRUE(std::holds_alternative<geom2d::Circle>(loop));
    EXPECT_EQ(std::get<geom2d::Circle>(loop).radius, 3.0);
}

TEST(Geom2dContract, ClippingResultsStartWithNothingFound) {
    // A default clipping keeps no stretch and lists nothing.
    const geom2d::RegionClipping clipping;
    EXPECT_TRUE(clipping.hosts.empty());
    EXPECT_TRUE(clipping.skippedHosts.empty());
    EXPECT_TRUE(clipping.skippedLoops.empty());
    const geom2d::ClippedHost host;
    EXPECT_TRUE(host.inside.empty());
    EXPECT_TRUE(host.boundary.empty());
    EXPECT_EQ(geom2d::BoundaryStretch{}.regionSide, geom2d::Side::left);
}
