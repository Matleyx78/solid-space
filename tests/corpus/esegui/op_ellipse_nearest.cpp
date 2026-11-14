// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "ellipse_data.hpp"
#include "operations.hpp"
#include "plane_data.hpp"
#include "solid_space/geom2d/ellipse.hpp"

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
// corners outward, that square lies inside the disc of radius error, so true
// means a point of the carrier within the error; the factor 0.7 keeps the
// rounded corners inside the disc.
bool onCarrier(const geom2d::Ellipse& ellipse, geom2d::Point2 point, double error) {
    const geom2d::Ellipse carrier{ellipse.center, ellipse.majorAxis, ellipse.ratio, 0.0,
                                  geom2d::kFullTurn};
    constexpr double kInsideDisc = 0.7;
    const double half = kInsideDisc * error;
    const double infinity = std::numeric_limits<double>::infinity();
    const geom2d::Point2 low{std::nextafter(point.x - half, infinity),
                             std::nextafter(point.y - half, infinity)};
    const geom2d::Point2 high{std::nextafter(point.x + half, -infinity),
                              std::nextafter(point.y + half, -infinity)};
    const auto meets = geom2d::meetsRectangle(carrier, low, high);
    if (!meets.hasValue()) {
        throw std::runtime_error("the point of nearest is beyond the domain of meetsRectangle");
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

}  // namespace
}  // namespace solid_space::corpus
