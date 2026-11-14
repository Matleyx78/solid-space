// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "operations.hpp"
#include "plane_data.hpp"
#include "solid_space/geom2d/clipping.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

namespace solid_space::corpus {
namespace {

Value writeEnd(const geom2d::StretchEnd& end) {
    return Value::group(
        {{"punto", writePoint(end.point)}, {"sull_ospite", writePosition(end.onHost)}});
}

Value writeHost(const geom2d::ClippedHost& host) {
    Value::List inside;
    for (const auto& stretch : host.inside) {
        inside.push_back(
            Value::group({{"da", writeEnd(stretch.from)}, {"a", writeEnd(stretch.to)}}));
    }
    Value::List boundary;
    for (const auto& stretch : host.boundary) {
        boundary.push_back(Value::group({{"da", writeEnd(stretch.from)},
                                         {"a", writeEnd(stretch.to)},
                                         {"lato", writeName(stretch.regionSide)}}));
    }
    return Value::group(
        {{"dentro", Value::list(std::move(inside))}, {"bordo", Value::list(std::move(boundary))}});
}

Value writeSkipped(std::span<const geom2d::SkippedInput> skipped) {
    Value::List values;
    for (const auto& entry : skipped) {
        values.push_back(
            Value::group({{"indice", Value::integer(static_cast<std::int64_t>(entry.index))},
                          {"diagnosi", field(writeDiagnosis(entry.diagnosis), "diagnosi")}}));
    }
    return Value::list(std::move(values));
}

const Value::List& requireList(const Value& value) {
    if (value.kind() != Value::Kind::list) {
        throw std::runtime_error("clipToRegion expects lists of hosts and loops");
    }
    return value.asList();
}

geom2d::Loop readLoop(const Value& value) {
    if (value.kind() == Value::Kind::group && value.find("cerchio") != nullptr) {
        return std::get<geom2d::Circle>(readCurve(value));
    }
    return readPolyline(value, "regione");
}

Value clippingOperation(const Value& input) {
    std::vector<geom2d::Curve> hosts;
    for (const auto& value : requireList(field(input, "ospiti"))) {
        hosts.push_back(readCurve(value, "ospiti"));
    }
    std::vector<geom2d::Loop> loops;
    for (const auto& value : requireList(field(input, "regione"))) {
        loops.push_back(readLoop(value));
    }
    const auto extension = readExtension(field(input, "prolunga_ospiti"));
    const auto result = geom2d::clipToRegion(hosts, extension, loops);
    Value::List clipped;
    std::size_t skippedIndex = 0;
    for (std::size_t i = 0; i < hosts.size(); ++i) {
        const auto alone = geom2d::clipToRegion(std::span{hosts}.subspan(i, 1), extension, loops);
        auto value = writeHost(result.hosts.at(i));
        const bool skipped = skippedIndex < result.skippedHosts.size() &&
                             result.skippedHosts[skippedIndex].index == i;
        if (alone.skippedHosts.size() != static_cast<std::size_t>(skipped) ||
            canonicalText(value) != canonicalText(writeHost(alone.hosts.at(0)))) {
            throw std::runtime_error("clipToRegion changes an isolated host in a batch");
        }
        if (skipped) {
            if (alone.skippedHosts.front().diagnosis.code !=
                result.skippedHosts[skippedIndex].diagnosis.code) {
                throw std::runtime_error(
                    "clipToRegion changes an isolated host's diagnosis in a batch");
            }
            ++skippedIndex;
        }
        clipped.push_back(std::move(value));
    }
    return Value::group({{"ospiti", Value::list(std::move(clipped))},
                         {"ospiti_scartati", writeSkipped(result.skippedHosts)},
                         {"loop_scartati", writeSkipped(result.skippedLoops)}});
}

const Registration kClipping("clipToRegion", clippingOperation);

}  // namespace
}  // namespace solid_space::corpus
