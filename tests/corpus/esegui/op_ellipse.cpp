// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "ellipse_data.hpp"
#include "operations.hpp"
#include "plane_data.hpp"
#include "solid_space/geom2d/ellipse.hpp"

#include <stdexcept>

namespace solid_space::corpus {
namespace {

Value magnitudeOperation(const Value& input) {
    const auto result = geom2d::magnitude(readEllipse(field(input, "ellisse")));
    if (!result.hasValue()) {
        return writeDiagnosis(result.diagnosis());
    }
    return Value::group({{"grandezza", Value::number(result.value())}});
}

Value endpointsOperation(const Value& input) {
    const auto result = geom2d::endpoints(readEllipse(field(input, "ellisse")));
    if (!result.hasValue()) {
        return writeDiagnosis(result.diagnosis());
    }
    return Value::group({
        {"inizio", writePoint(result.value().start)},
        {"fine", writePoint(result.value().end)},
    });
}

Value rectangleOperation(const Value& input) {
    const auto ellipse = readEllipse(field(input, "ellisse"));
    const auto first = readPoint(field(input, "angolo"), "angolo");
    const auto second = readPoint(field(input, "angolo_opposto"), "angolo_opposto");
    const auto result = geom2d::meetsRectangle(ellipse, first, second);
    if (!result.hasValue()) {
        return writeDiagnosis(result.diagnosis());
    }
    const auto swapped = geom2d::meetsRectangle(ellipse, second, first);
    if (!swapped.hasValue() || swapped.value() != result.value()) {
        throw std::runtime_error("meetsRectangle changed its answer with the corners swapped");
    }
    return Value::group({{"incontra", Value::boolean(result.value())}});
}

const Registration kEllipseMagnitude("ellipseMagnitude", magnitudeOperation);
const Registration kEllipseEndpoints("ellipseEndpoints", endpointsOperation);
const Registration kMeetsRectangle("meetsRectangle", rectangleOperation);

}  // namespace
}  // namespace solid_space::corpus
