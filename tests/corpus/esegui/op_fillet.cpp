// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// The cases of tangentCircleNear() and fillet() (stage 0.2, package P16;
// references [0.2-R16a], [0.2-R16b] and [0.2-R16c]). The keys are those of the
// package sheet: a chosen circle is written as an element of `cerchi` of
// tangentCircles (op_tangency.cpp), a fillet with its centre, radius, points
// of tangency, the semicircle flag, the arc in the form of curves and the
// bulge.

#include "operations.hpp"
#include "plane_data.hpp"
#include "solid_space/geom2d/tangency.hpp"

#include <stdexcept>

namespace solid_space::corpus {
namespace {

using geom2d::TangencyOutcome;
using geom2d::TangencyPoint;
using geom2d::TangentCircle;

Value writeOutcome(TangencyOutcome value) {
    switch (value) {
        case TangencyOutcome::found:
            return Value::name("found");
        case TangencyOutcome::none:
            return Value::name("none");
        case TangencyOutcome::infinitelyMany:
            return Value::name("infinitelyMany");
        case TangencyOutcome::ambiguousPick:
            return Value::name("ambiguousPick");
    }
    throw std::runtime_error("TangencyOutcome: unknown enumerator");
}

// As in op_tangency.cpp.
Value writeTangencyPoint(const TangencyPoint& point) {
    return Value::group(
        {{"punto", writePoint(point.point)}, {"sulla_curva", writePosition(point.position)}});
}

Value writeTangentCircle(const TangentCircle& circle) {
    return Value::group({{"centro", writePoint(circle.center)},
                         {"sulla_prima", writeTangencyPoint(circle.onFirst)},
                         {"sulla_seconda", writeTangencyPoint(circle.onSecond)},
                         {"lato_prima", writeName(circle.sideOfFirst)},
                         {"lato_seconda", writeName(circle.sideOfSecond)}});
}

Value tangentCircleNearOperation(const Value& input) {
    const auto result =
        geom2d::tangentCircleNear(readCurve(field(input, "prima"), "prima"),
                                  readPoint(field(input, "vicino_prima"), "vicino_prima"),
                                  readCurve(field(input, "seconda"), "seconda"),
                                  readPoint(field(input, "vicino_seconda"), "vicino_seconda"),
                                  readNumber(field(input, "raggio"), "raggio"));
    if (!result.hasValue()) {
        return writeDiagnosis(result.diagnosis());
    }
    const auto& chosen = result.value();
    if (chosen.outcome != TangencyOutcome::found) {
        return Value::group({{"esito", writeOutcome(chosen.outcome)}});
    }
    return Value::group(
        {{"esito", writeOutcome(chosen.outcome)}, {"cerchio", writeTangentCircle(chosen.circle)}});
}

Value filletOperation(const Value& input) {
    const auto result = geom2d::fillet(readCurve(field(input, "prima"), "prima"),
                                       readPoint(field(input, "punto_prima"), "punto_prima"),
                                       readCurve(field(input, "seconda"), "seconda"),
                                       readPoint(field(input, "punto_seconda"), "punto_seconda"),
                                       readNumber(field(input, "raggio"), "raggio"));
    if (!result.hasValue()) {
        return writeDiagnosis(result.diagnosis());
    }
    const auto& fillet = result.value();
    if (fillet.outcome != TangencyOutcome::found) {
        return Value::group({{"esito", writeOutcome(fillet.outcome)}});
    }
    return Value::group({{"esito", writeOutcome(fillet.outcome)},
                         {"centro", writePoint(fillet.center)},
                         {"raggio", Value::number(fillet.radius)},
                         {"sulla_prima", writeTangencyPoint(fillet.onFirst)},
                         {"sulla_seconda", writeTangencyPoint(fillet.onSecond)},
                         {"semicerchio", Value::boolean(fillet.semicircle)},
                         {"arco", writeCurve(fillet.arc)},
                         {"bulge", Value::number(fillet.bulge)}});
}

const Registration kTangentCircleNear("tangentCircleNear", tangentCircleNearOperation);
const Registration kFillet("fillet", filletOperation);

}  // namespace
}  // namespace solid_space::corpus
