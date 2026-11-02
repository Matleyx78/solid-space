// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// The cases of tangentsFrom() and tangentCircles() (stage 0.2, package P15;
// references [0.2-R15a] and [0.2-R15b]). Every case of tangentCircles also
// calls it with the curves swapped and fails unless it gives the same circles
// with the two curves swapped (tangency.hpp, tangentCircles; ADR 0011,
// "Verifica"): the same count, the positions and sides swapped exactly, and
// the centres, points and stations within twice their declared errors, since
// the contract does not say which double a construction returns (ADR 0013).

#include "operations.hpp"
#include "plane_data.hpp"
#include "solid_space/geom2d/curves.hpp"
#include "solid_space/geom2d/tangency.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace solid_space::corpus {
namespace {

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

[[noreturn]] void asymmetric(const std::string& what) {
    throw std::runtime_error("tangentCircles is not symmetric: " + what);
}

// Two returned values, each within relativeError * max(M, |v|) of the same
// exact value (curves.hpp), differ by at most twice that bound. The largest of
// the two returned magnitudes stands for |v| of the exact value, with a small
// margin for the difference between them.
bool closeEnough(double a, double b, double relativeError, double magnitude) {
    const double scale = std::max({magnitude, std::fabs(a), std::fabs(b)});
    return std::fabs(a - b) <= 2.0 * relativeError * scale * (1.0 + 0x1p-20);
}

// |p| of a point is its largest absolute coordinate (curves.hpp).
bool closeEnough(robust::Point2 a, robust::Point2 b, double magnitude) {
    const double scale =
        std::max({magnitude, std::fabs(a.x), std::fabs(a.y), std::fabs(b.x), std::fabs(b.y)});
    return closeEnough(a.x, b.x, geom2d::kRelativePointError, scale) &&
           closeEnough(a.y, b.y, geom2d::kRelativePointError, scale);
}

bool closeEnough(const TangencyPoint& a, const TangencyPoint& b, double magnitude) {
    return a.position.location == b.position.location && closeEnough(a.point, b.point, magnitude) &&
           closeEnough(a.position.station, b.position.station, geom2d::kRelativeStationError,
                       magnitude);
}

bool sameSwapped(const TangentCircle& circle, const TangentCircle& other, double magnitude) {
    return circle.sideOfFirst == other.sideOfSecond && circle.sideOfSecond == other.sideOfFirst &&
           closeEnough(circle.center, other.center, magnitude) &&
           closeEnough(circle.onFirst, other.onSecond, magnitude) &&
           closeEnough(circle.onSecond, other.onFirst, magnitude);
}

// The magnitude M of tangentCircles (tangency.hpp): the largest among the
// magnitudes of the two curves and the radius. Only called after both calls
// succeeded, so the data are valid.
double operationMagnitude(const geom2d::Curve& first, const geom2d::Curve& second, double radius) {
    const auto a = geom2d::magnitude(first);
    const auto b = geom2d::magnitude(second);
    if (!a.hasValue() || !b.hasValue()) {
        throw std::runtime_error("tangentCircles succeeded on curves without a magnitude");
    }
    return std::max({a.value(), b.value(), radius});
}

// The diagnosis of the swapped call (curves.hpp checks the inputs in order):
// the radius and the constructions (input 2) give the same diagnosis; a
// wrong curve gives the same code on its new input, unless both curves are
// wrong, when each call names its own first curve.
void checkSymmetricDiagnoses(const geom2d::Diagnosis& a, const geom2d::Diagnosis& b) {
    bool same = false;
    if (a.input >= 2) {
        same = b.input == a.input && b.code == a.code;
    } else if (a.input == 1) {
        same = b.input == 0 && b.code == a.code;
    } else {
        same = (b.input == 1 && b.code == a.code) || b.input == 0;
    }
    if (!same) {
        asymmetric("the diagnoses differ");
    }
}

// Whether every circle of the direct call can be matched with a different
// circle of the swapped call. With tolerances a circle may fit more than one
// (#162), so every assignment is tried: at most eight circles, 8! orders.
bool completeMatching(const std::vector<std::vector<bool>>& fits) {
    std::vector<std::size_t> order(fits.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    for (bool more = true; more; more = std::next_permutation(order.begin(), order.end())) {
        bool all = true;
        for (std::size_t i = 0; i < order.size() && all; ++i) {
            all = fits[i][order[i]];
        }
        if (all) {
            return true;
        }
    }
    return false;
}

void checkSymmetry(const geom2d::Result<TangentCircles>& direct,
                   const geom2d::Result<TangentCircles>& swapped, double magnitude) {
    if (direct.hasValue() != swapped.hasValue()) {
        asymmetric("only one of the two calls gives a diagnosis");
    }
    if (!direct.hasValue()) {
        checkSymmetricDiagnoses(direct.diagnosis(), swapped.diagnosis());
        return;
    }
    const auto& a = direct.value();
    const auto& b = swapped.value();
    if (a.infinitelyMany != b.infinitelyMany || a.circles.size() != b.circles.size()) {
        asymmetric("the counts differ");
    }
    std::vector<std::vector<bool>> fits(a.circles.size(),
                                        std::vector<bool>(b.circles.size(), false));
    for (std::size_t i = 0; i < a.circles.size(); ++i) {
        for (std::size_t j = 0; j < b.circles.size(); ++j) {
            fits[i][j] = sameSwapped(a.circles[i], b.circles[j], magnitude);
        }
    }
    if (!completeMatching(fits)) {
        asymmetric("a circle is missing with the curves swapped");
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
    if (!direct.hasValue() || !swapped.hasValue()) {
        checkSymmetry(direct, swapped, 0.0);
        return writeDiagnosis(direct.diagnosis());
    }
    checkSymmetry(direct, swapped, operationMagnitude(left, right, radius));
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
