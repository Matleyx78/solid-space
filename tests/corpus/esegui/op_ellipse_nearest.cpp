// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "ellipse_data.hpp"
#include "operations.hpp"
#include "plane_data.hpp"
#include "solid_space/geom2d/ellipse.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace solid_space::corpus {
namespace {

// The error of nearest(), kRelativeNearestError * N, with N the largest of
// the magnitude of the ellipse and of the coordinates of the point.
double nearestError(const geom2d::Ellipse& ellipse, geom2d::Point2 point) {
    const auto magnitude = geom2d::magnitude(ellipse);
    if (!magnitude.hasValue()) {
        throw std::runtime_error("nearest succeeded on an ellipse without a magnitude");
    }
    return geom2d::kRelativeNearestError *
           std::max({magnitude.value(), std::fabs(point.x), std::fabs(point.y)});
}

// True when the carrying ellipse (centre, axis, ratio) meets the closed
// square [low, high], decided exactly by meetsRectangle() on the implicit
// equation.
bool carrierMeets(geom2d::Point2 center, geom2d::Point2 axis, double ratio, geom2d::Point2 low,
                  geom2d::Point2 high) {
    const geom2d::Ellipse carrier{center, axis, ratio, 0.0, geom2d::kFullTurn};
    const auto meets = geom2d::meetsRectangle(carrier, low, high);
    if (!meets.hasValue()) {
        throw std::runtime_error("meetsRectangle refused the square of nearest");
    }
    return meets.value();
}

// The square of half side `half` around `centre`, its corners rounded inward.
std::pair<geom2d::Point2, geom2d::Point2> squareAround(geom2d::Point2 centre, double half) {
    const double infinity = std::numeric_limits<double>::infinity();
    return {
        {std::nextafter(centre.x - half, infinity), std::nextafter(centre.y - half, infinity)},
        {std::nextafter(centre.x + half, -infinity), std::nextafter(centre.y + half, -infinity)}};
}

// "sulla_curva": true only when the point is within the error of the
// carrying ellipse (the whole ellipse with the same centre, axis and ratio).
// meetsRectangle() decides exactly whether the carrier has a point in a
// closed square around the point that lies inside the disc of radius error.
// - Inside the domain the square has half side 0.7 error: its half diagonal
//   is below the error, with room for the rounding of the corners.
// - The point of nearest() may lie beyond robust::kMaxCoordinate
//   (ellipse.hpp), where meetsRectangle() refuses it. Then the figure is
//   moved so that the centre is the origin, which keeps the axis and the
//   ratio, so the carrier, as they are: d = point - centre in doubles is
//   within half an ulp of a number at most 4 N (2^-51 N), and the corners
//   of the square of half side 0.6 error around d within an ulp more; its
//   corners are then within 0.85 error + 2^-49 N < error of the exact
//   point, also moved. A point farther from the centre than any point of
//   the carrier (2 (|a.x| + |a.y|)) by more than twice the error is not on
//   it. Otherwise, if the square or the axis still goes beyond the limit,
//   the axis is at least 2^47 long in one coordinate, and everything is
//   multiplied by a power of two below one: exact except for a coordinate
//   of the axis below 2^-1072, whose loss moves the carrier by less than
//   2^-1072, nothing beside the room of the square; the axis never becomes
//   zero.
bool onCarrier(const geom2d::Ellipse& ellipse, geom2d::Point2 point, double error) {
    constexpr double kInsideDisc = 0.7;
    const double half = kInsideDisc * error;
    const auto& center = ellipse.center;
    const auto& axis = ellipse.majorAxis;
    const double reach =
        std::max({std::fabs(point.x) + half, std::fabs(point.y) + half, std::fabs(center.x),
                  std::fabs(center.y), std::fabs(axis.x), std::fabs(axis.y)});
    if (reach <= robust::kMaxCoordinate) {
        const auto [low, high] = squareAround(point, half);
        return carrierMeets(center, axis, ellipse.ratio, low, high);
    }

    constexpr double kMovedInsideDisc = 0.6;
    const double movedHalf = kMovedInsideDisc * error;
    const geom2d::Point2 moved{point.x - center.x, point.y - center.y};
    constexpr double kWiderThanCarrier = 2.001;
    const double carrierReach = kWiderThanCarrier * (std::fabs(axis.x) + std::fabs(axis.y));
    if (std::max(std::fabs(moved.x), std::fabs(moved.y)) > carrierReach + 2.0 * error) {
        return false;
    }
    const double movedReach =
        std::max({std::fabs(moved.x) + movedHalf, std::fabs(moved.y) + movedHalf, std::fabs(axis.x),
                  std::fabs(axis.y)});
    int exponent = 0;
    while (std::ldexp(movedReach, exponent) > robust::kMaxCoordinate) {
        --exponent;
    }
    const auto scaled = [exponent](geom2d::Point2 p) {
        return geom2d::Point2{std::ldexp(p.x, exponent), std::ldexp(p.y, exponent)};
    };
    const auto [low, high] = squareAround(scaled(moved), std::ldexp(movedHalf, exponent));
    return carrierMeets({0.0, 0.0}, scaled(axis), ellipse.ratio, low, high);
}

Value nearestOperation(const Value& input) {
    const auto ellipse = readEllipse(field(input, "ellisse"));
    const auto point = readPoint(field(input, "punto"), "punto");
    const auto result = geom2d::nearest(ellipse, point);
    if (!result.hasValue()) {
        return writeDiagnosis(result.diagnosis());
    }
    const auto& nearest = result.value();
    return Value::group({
        {"punto", writePoint(nearest.point)},
        {"distanza", Value::number(nearest.distance)},
        {"posizione", writeEllipseLocation(nearest.location)},
        {"sulla_curva",
         Value::boolean(onCarrier(ellipse, nearest.point, nearestError(ellipse, point)))},
    });
}

const Registration kEllipseNearest("nearest", nearestOperation);

// The point of nearest() beyond robust::kMaxCoordinate (review of #289): an
// arc whose centre is on the limit, seen from its centre. The operation must
// answer, with "sulla_curva" true, instead of throwing.
TEST(EllipseNearestOperation, PointBeyondTheLimitOfCoordinates) {
    const double limit = robust::kMaxCoordinate;
    const auto input = Value::group({
        {"ellisse", Value::group({
                        {"centro", writePoint({limit, 0.0})},
                        {"asse_maggiore", writePoint({1024.0, 0.0})},
                        {"rapporto", Value::number(0.5)},
                        {"parametro_iniziale", Value::number(0.0)},
                        {"parametro_finale", Value::number(0.125)},
                    })},
        {"punto", writePoint({limit, 0.0})},
    });
    const auto output = nearestOperation(input);
    const auto* point = output.find("punto");
    ASSERT_NE(point, nullptr);
    EXPECT_GT(readPoint(*point, "punto").x, limit);
    const auto* onCurve = output.find("sulla_curva");
    ASSERT_NE(onCurve, nullptr);
    EXPECT_TRUE(onCurve->asBoolean());
}

// The same with an axis of one subnormal (second review of #289): the
// figure is moved, not scaled, so the axis keeps its only bit.
TEST(EllipseNearestOperation, SubnormalAxisAtTheLimitOfCoordinates) {
    const double limit = robust::kMaxCoordinate;
    const auto input = Value::group({
        {"ellisse", Value::group({
                        {"centro", writePoint({limit, 0.0})},
                        {"asse_maggiore", writePoint({0x1p-1074, 0.0})},
                        {"rapporto", Value::number(0.5)},
                        {"parametro_iniziale", Value::number(0.0)},
                        {"parametro_finale", Value::number(0.125)},
                    })},
        {"punto", writePoint({limit, 0.0})},
    });
    const auto output = nearestOperation(input);
    const auto* onCurve = output.find("sulla_curva");
    ASSERT_NE(onCurve, nullptr);
    EXPECT_TRUE(onCurve->asBoolean());
}

// A point far from a large carrier beyond the limit is not on it, and one on
// it is, whichever way the check goes.
TEST(EllipseNearestOperation, OnCarrierBeyondTheLimitOfCoordinates) {
    const double limit = robust::kMaxCoordinate;
    const geom2d::Ellipse large{{limit, 0.0}, {limit, 0.0}, 0.5, 0.0, geom2d::kFullTurn};
    const double error = geom2d::kRelativeNearestError * 2.0 * limit;
    EXPECT_TRUE(onCarrier(large, {2.0 * limit, 0.0}, error));
    EXPECT_FALSE(onCarrier(large, {2.0 * limit + 4.0 * error, 0.0}, error));
    const geom2d::Ellipse tiny{{limit, 0.0}, {0x1p-1074, 0.0}, 0.5, 0.0, geom2d::kFullTurn};
    EXPECT_TRUE(onCarrier(tiny, {limit, 0.0}, error));
    EXPECT_FALSE(onCarrier(tiny, {limit - 4.0 * error, 0.0}, error));
}

}  // namespace
}  // namespace solid_space::corpus
