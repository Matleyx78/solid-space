// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// The cases of tangentsFrom() and tangentCircles() (stage 0.2, package P15;
// references [0.2-R15a] and [0.2-R15b]). Every case of tangentCircles also
// calls it with the curves swapped and fails unless it gives the same circles,
// with the same bits, with the two curves and their sides swapped (ADR 0011,
// "Verifica"): the points and stations are correctly rounded values of exact
// numbers, so the order of the arguments cannot change them.

#include "operations.hpp"
#include "plane_data.hpp"
#include "solid_space/geom2d/tangency.hpp"

#include <bit>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace solid_space::corpus {
namespace {

using geom2d::CurvePosition;
using geom2d::Placement;
using geom2d::TangencyPoint;
using geom2d::TangentCircle;
using geom2d::TangentCircles;

Value writePlacement(Placement value) {
    switch (value) {
        case Placement::outside:
            return Value::name("outside");
        case Placement::on:
            return Value::name("on");
        case Placement::inside:
            return Value::name("inside");
    }
    throw std::runtime_error("Placement: unknown enumerator");
}

Value writeTangencyPoint(const TangencyPoint& point) {
    return Value::group(
        {{"punto", writePoint(point.point)}, {"sulla_curva", writePosition(point.position)}});
}

bool sameBits(double a, double b) {
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

bool sameBits(robust::Point2 a, robust::Point2 b) {
    return sameBits(a.x, b.x) && sameBits(a.y, b.y);
}

bool sameBits(const CurvePosition& a, const CurvePosition& b) {
    return a.location == b.location && sameBits(a.station, b.station);
}

bool sameBits(const TangencyPoint& a, const TangencyPoint& b) {
    return sameBits(a.point, b.point) && sameBits(a.position, b.position);
}

[[noreturn]] void asymmetric(const std::string& what) {
    throw std::runtime_error("tangentCircles is not symmetric: " + what);
}

bool sameSwapped(const TangentCircle& circle, const TangentCircle& other) {
    return sameBits(circle.center, other.center) && sameBits(circle.onFirst, other.onSecond) &&
           sameBits(circle.onSecond, other.onFirst) && circle.sideOfFirst == other.sideOfSecond &&
           circle.sideOfSecond == other.sideOfFirst;
}

void checkSymmetry(const geom2d::Result<TangentCircles>& direct,
                   const geom2d::Result<TangentCircles>& swapped) {
    if (direct.hasValue() != swapped.hasValue()) {
        asymmetric("only one of the two calls gives a diagnosis");
    }
    if (!direct.hasValue()) {
        const auto& a = direct.diagnosis();
        const auto& b = swapped.diagnosis();
        if (a.input + b.input == 1 && a.code != b.code) {
            asymmetric("the diagnoses differ");
        }
        return;
    }
    const auto& a = direct.value();
    const auto& b = swapped.value();
    if (a.infinitelyMany != b.infinitelyMany || a.circles.size() != b.circles.size()) {
        asymmetric("the counts differ");
    }
    // The circles are different, so each one has a single match.
    for (const auto& circle : a.circles) {
        bool found = false;
        for (const auto& other : b.circles) {
            found = found || sameSwapped(circle, other);
        }
        if (!found) {
            asymmetric("a circle is missing with the curves swapped");
        }
    }
}

Value tangentsFromOperation(const Value& input) {
    const auto result =
        geom2d::tangentsFrom(readPoint(field(input, "punto")), readCurve(field(input, "curva")),
                             readExtension(field(input, "prolunga"), "prolunga"));
    if (!result.hasValue()) {
        return writeDiagnosis(result.diagnosis());
    }
    Value::List points;
    for (const auto& point : result.value().points) {
        points.push_back(writeTangencyPoint(point));
    }
    return Value::group({{"collocazione", writePlacement(result.value().placement)},
                         {"punti", Value::list(std::move(points))}});
}

Value tangentCirclesOperation(const Value& input) {
    // Named apart from the parameters of tangentCircles, which the swapped call
    // crosses on purpose.
    const auto left = readCurve(field(input, "prima"), "prima");
    const auto right = readCurve(field(input, "seconda"), "seconda");
    const double radius = readNumber(field(input, "raggio"), "raggio");
    const auto direct = geom2d::tangentCircles(left, right, radius);
    const auto swapped = geom2d::tangentCircles(right, left, radius);
    checkSymmetry(direct, swapped);
    if (!direct.hasValue()) {
        return writeDiagnosis(direct.diagnosis());
    }
    Value::List circles;
    for (const auto& circle : direct.value().circles) {
        circles.push_back(Value::group({{"centro", writePoint(circle.center)},
                                        {"sulla_prima", writeTangencyPoint(circle.onFirst)},
                                        {"sulla_seconda", writeTangencyPoint(circle.onSecond)},
                                        {"lato_prima", writeName(circle.sideOfFirst)},
                                        {"lato_seconda", writeName(circle.sideOfSecond)}}));
    }
    return Value::group({{"infiniti", Value::boolean(direct.value().infinitelyMany)},
                         {"cerchi", Value::list(std::move(circles))}});
}

const Registration kTangentsFrom("tangentsFrom", tangentsFromOperation);
const Registration kTangentCircles("tangentCircles", tangentCirclesOperation);

}  // namespace
}  // namespace solid_space::corpus
