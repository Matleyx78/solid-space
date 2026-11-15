// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "region_parts.hpp"
#include "solid_space/geom2d/boundary.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace solid_space::geom2d {
namespace {

struct OffLoopParts {
    bool inside = false;
    bool outside = false;

    [[nodiscard]] bool mixed() const { return inside && outside; }

    [[nodiscard]] bool allOnLoop() const { return !inside && !outside; }
};

OffLoopParts offLoopParts(const detail::LoopPieces& host, const detail::LoopPieces& loop) {
    OffLoopParts offLoop;
    for (const auto& piece : host.pieces) {
        const auto parts = detail::regionParts(piece.meaning, Extension::none, std::span{&loop, 1});
        for (std::size_t i = 0; i < parts.classes.size(); ++i) {
            if (parts.onLoop.at(i)) {
                continue;
            }
            // Points away from the loop have the same parity on both sides.
            offLoop.inside |= parts.classes.at(i) == detail::PartClass::inside;
            offLoop.outside |= parts.classes.at(i) == detail::PartClass::outside;
        }
        if (offLoop.mixed()) {
            break;
        }
    }
    return offLoop;
}

void countEnclosures(ContourNesting& nesting, std::span<const detail::LoopPieces> loops,
                     std::size_t first, std::size_t second) {
    const auto& a = loops[first];
    const auto& b = loops[second];
    const auto aInB = offLoopParts(a, b);
    const auto bInA = offLoopParts(b, a);
    if (aInB.allOnLoop() && bInA.allOnLoop()) {
        nesting.conflicts.push_back({first, second, ContourConflictKind::same});
    } else if (aInB.mixed() || bInA.mixed() || (aInB.inside && bInA.inside)) {
        nesting.conflicts.push_back({first, second, ContourConflictKind::crossing});
    } else {
        auto& firstDepth = nesting.depth.at(first);
        auto& secondDepth = nesting.depth.at(second);
        firstDepth = firstDepth.value_or(0) + static_cast<std::size_t>(aInB.inside);
        secondDepth = secondDepth.value_or(0) + static_cast<std::size_t>(bInA.inside);
    }
}

}  // namespace

ContourNesting nestContours(std::span<const Loop> contours) {
    ContourNesting result;
    result.depth.resize(contours.size());
    std::vector<detail::LoopPieces> loops(contours.size());
    std::vector<std::size_t> kept;
    for (std::size_t i = 0; i < contours.size(); ++i) {
        auto checked = detail::checkedLoopPieces(contours[i], i);
        if (checked.hasValue()) {
            loops.at(i) = std::move(checked).value();
            result.depth.at(i) = 0;
            kept.push_back(i);
        } else {
            result.skipped.push_back({i, checked.diagnosis()});
        }
    }
    for (std::size_t i = 0; i < kept.size(); ++i) {
        for (std::size_t j = i + 1; j < kept.size(); ++j) {
            countEnclosures(result, loops, kept.at(i), kept.at(j));
        }
    }
    return result;
}

}  // namespace solid_space::geom2d
