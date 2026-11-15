// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "operations.hpp"
#include "plane_data.hpp"
#include "solid_space/geom2d/boundary.hpp"

#include <algorithm>
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

Value writeNesting(const geom2d::ContourNesting& result) {
    Value::List depths;
    for (const auto depth : result.depth) {
        depths.push_back(depth ? writeIndex(*depth) : Value::boolean(false));
    }
    Value::List conflicts;
    for (const auto& [first, second, kind] : result.conflicts) {
        const auto* name = kind == geom2d::ContourConflictKind::same ? "same" : "crossing";
        conflicts.push_back(Value::group({{"primo", writeIndex(first)},
                                          {"secondo", writeIndex(second)},
                                          {"tipo", Value::name(name)}}));
    }
    Value::List skipped;
    for (const auto& [index, diagnosis] : result.skipped) {
        const auto written = writeDiagnosis(diagnosis);
        skipped.push_back(Value::group(
            {{"indice", writeIndex(index)}, {"diagnosi", field(written, "diagnosi")}}));
    }
    return Value::group({{"profondita", Value::list(std::move(depths))},
                         {"conflitti", Value::list(std::move(conflicts))},
                         {"scartati", Value::list(std::move(skipped))}});
}

void restoreIndices(geom2d::ContourNesting& result, std::size_t size) {
    const auto originalIndex = [size](std::size_t index) {
        return size - 1 - index;
    };
    std::ranges::reverse(result.depth);
    std::ranges::reverse(result.skipped);
    for (auto& [index, diagnosis] : result.skipped) {
        index = originalIndex(index);
        diagnosis.input = originalIndex(diagnosis.input);
    }
    for (auto& [first, second, kind] : result.conflicts) {
        const auto originalFirst = originalIndex(second);
        second = originalIndex(first);
        first = originalFirst;
    }
    std::ranges::sort(result.conflicts, {}, [](const geom2d::ContourConflict& conflict) {
        return std::pair{conflict.first, conflict.second};
    });
}

Value nestContoursOperation(const Value& input) {
    const auto& list = field(input, "contorni");
    if (list.kind() != Value::Kind::list) {
        throw std::runtime_error("nestContours expects a list of contours");
    }
    std::vector<geom2d::Loop> loops;
    for (const auto& value : list.asList()) {
        if (value.kind() == Value::Kind::group && value.find("cerchio") != nullptr) {
            loops.emplace_back(std::get<geom2d::Circle>(readCurve(value, "contorni")));
        } else {
            loops.emplace_back(readPolyline(value, "contorni"));
        }
    }

    auto result = writeNesting(geom2d::nestContours(loops));
    std::ranges::reverse(loops);
    auto reversed = geom2d::nestContours(loops);
    restoreIndices(reversed, loops.size());
    if (canonicalText(result) != canonicalText(writeNesting(reversed))) {
        throw std::runtime_error("nestContours changes when the contours are reversed");
    }
    return result;
}

const Registration kNestContours("nestContours", nestContoursOperation);

}  // namespace
}  // namespace solid_space::corpus
