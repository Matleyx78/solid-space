// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "operations.hpp"
#include "plane_data.hpp"
#include "solid_space/geom2d/boundary.hpp"
#include "solid_space/robust/tolerance.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace solid_space::corpus {
namespace {

Value writeIndex(std::size_t index) {
    return Value::integer(static_cast<std::int64_t>(index));
}

Value writeOutcome(geom2d::BoundaryOutcome outcome) {
    switch (outcome) {
        case geom2d::BoundaryOutcome::enclosed:
            return Value::name("enclosed");
        case geom2d::BoundaryOutcome::onCurve:
            return Value::name("onCurve");
        case geom2d::BoundaryOutcome::notEnclosed:
            break;
    }
    return Value::name("notEnclosed");
}

Value writeLoop(const geom2d::Loop& loop) {
    if (const auto* circle = std::get_if<geom2d::Circle>(&loop)) {
        return Value::group(
            {{"cerchio", Value::group({{"centro", writePoint(circle->center)},
                                       {"raggio", Value::number(circle->radius)}})}});
    }
    const auto& polyline = std::get<geom2d::Polyline>(loop);
    Value::List vertices;
    for (const auto& vertex : polyline.vertices) {
        vertices.push_back(
            Value::list({Value::number(vertex.position.x), Value::number(vertex.position.y),
                         Value::number(vertex.bulge)}));
    }
    return Value::group({{"vertici", Value::list(std::move(vertices))},
                         {"chiusa", Value::boolean(polyline.closed)}});
}

Value writeOrigin(const geom2d::LoopPieceOrigin& origin) {
    const auto* source = origin.source == geom2d::LoopPieceSource::link ? "link" : "curve";
    return Value::group({{"fonte", Value::name(source)},
                         {"curva", writeIndex(origin.curve)},
                         {"altra_curva", writeIndex(origin.otherCurve)},
                         {"invertito", Value::boolean(origin.reversed)}});
}

Value writeBoundary(const geom2d::DetectedBoundary& result) {
    Value::List loops;
    for (const auto& loop : result.loops) {
        Value::List origins;
        for (const auto& origin : loop.origins) {
            origins.push_back(writeOrigin(origin));
        }
        loops.push_back(Value::group({{"loop", writeLoop(loop.loop)},
                                      {"origini", Value::list(std::move(origins))},
                                      {"profondita", writeIndex(loop.depth)}}));
    }
    Value::List ends;
    for (const auto& end : result.openEnds) {
        ends.push_back(Value::group({{"curva", writeIndex(end.curve)},
                                     {"all_inizio", Value::boolean(end.atStart)},
                                     {"punto", writePoint(end.point)}}));
    }
    Value::List skipped;
    for (const auto& [index, diagnosis] : result.skipped) {
        const auto written = writeDiagnosis(diagnosis);
        skipped.push_back(Value::group(
            {{"indice", writeIndex(index)}, {"diagnosi", field(written, "diagnosi")}}));
    }
    Value::Group fields{{"esito", writeOutcome(result.outcome)},
                        {"loop", Value::list(std::move(loops))},
                        {"estremi_liberi", Value::list(std::move(ends))},
                        {"scartate", Value::list(std::move(skipped))}};
    if (result.outcome == geom2d::BoundaryOutcome::onCurve) {
        fields.emplace_back("curva_vicina", writeIndex(result.nearCurve));
    }
    return Value::group(std::move(fields));
}

Value detectBoundaryOperation(const Value& input) {
    const auto picked = readPoint(field(input, "punto"), "punto");
    const auto& list = field(input, "curve");
    if (list.kind() != Value::Kind::list) {
        throw std::runtime_error("curve: expected a list of curves");
    }
    std::vector<geom2d::Curve> curves;
    for (const auto& value : list.asList()) {
        curves.push_back(readCurve(value, "curve"));
    }
    const auto policy = robust::TolerancePolicy::forUnit(
        readNumber(field(input, "unita_in_metri"), "unita_in_metri"));
    if (!policy.hasValue()) {
        throw std::runtime_error("unita_in_metri: invalid unit: " + policy.diagnosis().message);
    }
    const auto result = geom2d::detectBoundary(picked, curves, policy.value());
    if (!result.hasValue()) {
        return writeDiagnosis(result.diagnosis());
    }
    return writeBoundary(result.value());
}

const Registration kDetectBoundary("detectBoundary", detectBoundaryOperation);

}  // namespace
}  // namespace solid_space::corpus
