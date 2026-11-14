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

// "sulla_curva": true when the point is within the error of the carrying
// ellipse, decided exactly with the implicit equation. The carrying ellipse
// is the whole ellipse with the same centre, axis and ratio; meetsRectangle()
// decides exactly whether it has a point in the closed square of half side
// h around the point. With h at most error / sqrt(2) after rounding the
// corners inward, that square lies inside the disc of radius error, so true
// means a point of the carrier within the error; the factor 0.7 keeps the
// rounded corners inside the disc.
// The point of nearest() may lie beyond robust::kMaxCoordinate (ellipse.hpp),
// where meetsRectangle() refuses it. Then the centre, the axis, the point and
// the error are all multiplied by a power of two below one: exact for doubles
// of this size, so the carrier and the square are the same figure at a
// smaller scale and the answer does not change.
bool onCarrier(const geom2d::Ellipse& ellipse, geom2d::Point2 point, double error) {
    constexpr double kInsideDisc = 0.7;
    const double half = kInsideDisc * error;
    const double reach = std::max({std::fabs(point.x) + half, std::fabs(point.y) + half,
                                   std::fabs(ellipse.center.x), std::fabs(ellipse.center.y),
                                   std::fabs(ellipse.majorAxis.x), std::fabs(ellipse.majorAxis.y)});
    int exponent = 0;
    while (std::ldexp(reach, exponent) > robust::kMaxCoordinate) {
        --exponent;
    }
    const auto scaled = [exponent](geom2d::Point2 p) {
        return geom2d::Point2{std::ldexp(p.x, exponent), std::ldexp(p.y, exponent)};
    };
    const geom2d::Ellipse carrier{scaled(ellipse.center), scaled(ellipse.majorAxis), ellipse.ratio,
                                  0.0, geom2d::kFullTurn};
    const auto centre = scaled(point);
    const double scaledHalf = std::ldexp(half, exponent);
    const double infinity = std::numeric_limits<double>::infinity();
    const geom2d::Point2 low{std::nextafter(centre.x - scaledHalf, infinity),
                             std::nextafter(centre.y - scaledHalf, infinity)};
    const geom2d::Point2 high{std::nextafter(centre.x + scaledHalf, -infinity),
                              std::nextafter(centre.y + scaledHalf, -infinity)};
    const auto meets = geom2d::meetsRectangle(carrier, low, high);
    if (!meets.hasValue()) {
        throw std::runtime_error("meetsRectangle refused the scaled square of nearest");
    }
    return meets.value();
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

}  // namespace
}  // namespace solid_space::corpus
