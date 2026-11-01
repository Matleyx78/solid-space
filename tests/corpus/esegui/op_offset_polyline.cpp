// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "operations.hpp"
#include "plane_data.hpp"
#include "solid_space/geom2d/offset.hpp"
#include "solid_space/robust/tolerance.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

namespace solid_space::corpus {
namespace {

robust::TolerancePolicy policyOf(const Value& input) {
    const auto policy = robust::TolerancePolicy::forUnit(
        readNumber(field(input, "unita_in_metri"), "unita_in_metri"));
    if (!policy.hasValue()) {
        throw std::runtime_error("unita_in_metri: invalid unit: " + policy.diagnosis().message);
    }
    return policy.value();
}

Value writeName(geom2d::OffsetOutcome value) {
    switch (value) {
        case geom2d::OffsetOutcome::done:
            return Value::name("done");
        case geom2d::OffsetOutcome::vanished:
            return Value::name("vanished");
        case geom2d::OffsetOutcome::failed:
            break;
    }
    return Value::name("failed");
}

Value writeName(geom2d::OffsetFailure value) {
    switch (value) {
        case geom2d::OffsetFailure::none:
            return Value::name("none");
        case geom2d::OffsetFailure::openJoint:
            return Value::name("openJoint");
        case geom2d::OffsetFailure::beyondDomain:
            return Value::name("beyondDomain");
        case geom2d::OffsetFailure::selfIntersection:
            return Value::name("selfIntersection");
        case geom2d::OffsetFailure::meetsOriginal:
            return Value::name("meetsOriginal");
        case geom2d::OffsetFailure::notRepresentable:
            break;
    }
    return Value::name("notRepresentable");
}

Value writeName(geom2d::PieceOrigin value) {
    return Value::name(value == geom2d::PieceOrigin::source ? "source" : "gap");
}

Value writePolyline(const geom2d::Polyline& polyline) {
    Value::List vertices;
    for (const auto& vertex : polyline.vertices) {
        vertices.push_back(
            Value::list({Value::number(vertex.position.x), Value::number(vertex.position.y),
                         Value::number(vertex.bulge)}));
    }
    return Value::group({{"vertici", Value::list(std::move(vertices))},
                         {"chiusa", Value::boolean(polyline.closed)}});
}

Value offsetPolylineOperation(const Value& input) {
    const auto polyline = readPolyline(field(input, "polilinea"));
    const auto distance = readNumber(field(input, "distanza"));
    const auto result = geom2d::offsetPolyline(polyline, distance, policyOf(input));
    if (!result.hasValue()) {
        return writeDiagnosis(result.diagnosis());
    }
    const auto& offset = result.value();
    Value::Group fields{{"esito", writeName(offset.outcome)},
                        {"motivo", writeName(offset.failure)},
                        {"semplice", Value::boolean(offset.simple)}};
    if (offset.outcome == geom2d::OffsetOutcome::done) {
        fields.emplace_back("polilinea", writePolyline(offset.polyline));
        Value::List origins;
        for (const auto& origin : offset.origins) {
            origins.push_back(Value::group(
                {{"origine", writeName(origin.origin)},
                 {"indice", Value::integer(static_cast<std::int64_t>(origin.index))}}));
        }
        fields.emplace_back("origini", Value::list(std::move(origins)));
    }
    return Value::group(std::move(fields));
}

const Registration kOffsetPolyline("offsetPolyline", offsetPolylineOperation);

}  // namespace
}  // namespace solid_space::corpus
