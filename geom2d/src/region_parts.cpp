// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "region_parts.hpp"

#include "local_order.hpp"
#include "positions.hpp"
#include "solid_space/robust/predicates.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <utility>
#include <variant>

namespace solid_space::geom2d::detail {
namespace {

bool fullCircle(const CurveMeaning& meaning) {
    const auto* arc = std::get_if<ArcMeaning>(&meaning.support);
    return arc != nullptr && (arc->span == ArcSpan::circle || arc->span == ArcSpan::whole);
}

bool circleFits(const CircleSupport& circle) {
    // Each axis needs room for the exact radius beyond the absolute centre.
    const auto limit = Dyadic(robust::kMaxCoordinate) * circle.weight;
    const auto maxCentre = compare(circle.centerX.abs(), circle.centerY.abs()) == Sign::positive
                               ? circle.centerX.abs()
                               : circle.centerY.abs();
    const auto room = limit - maxCentre;
    return room.sign() != Sign::negative &&
           compare(room * room, circle.radiusSquared) != Sign::negative;
}

std::optional<Diagnosis> appendPiece(LoopPieces& loop, const Curve& curve, std::size_t index,
                                     std::size_t input) {
    auto checked = checkedMeaning(curve, input, Degenerate::accept);
    if (!checked.hasValue()) {
        return checked.diagnosis();
    }
    auto meaning = std::move(checked).value();
    if (meaning.degenerate) {
        return std::nullopt;
    }
    auto carrier = carrierOf(meaning);
    if (const auto* circle = std::get_if<CircleSupport>(&carrier);
        circle != nullptr && !circleFits(*circle)) {
        return Diagnosis{DiagnosisCode::outOfDomain, input,
                         "the carrying circle exceeds the coordinate domain"};
    }
    loop.pieces.push_back(LoopPiece{index, std::move(meaning), std::move(carrier)});
    return std::nullopt;
}

bool validPoint(Point2 point) {
    return std::isfinite(point.x) && std::isfinite(point.y) &&
           std::abs(point.x) <= robust::kMaxCoordinate &&
           std::abs(point.y) <= robust::kMaxCoordinate;
}

void addCuts(std::vector<ExactPoint>& cuts, const CurveMeaning& host, const Carrier& carrier,
             Extension extension, const LoopPiece& piece) {
    const auto meeting = meet(carrier, piece.carrier);
    if (meeting.sameCarrier && !fullCircle(piece.meaning)) {
        for (const auto& end : {startPoint(piece.meaning), endPoint(piece.meaning)}) {
            if (takesPart(locate(host, end), extension)) {
                cuts.push_back(end);
            }
        }
    }
    for (const auto& contact : meeting.found()) {
        if (takesPart(locate(host, contact.point), extension) &&
            takesPart(locate(piece.meaning, contact.point), Extension::none)) {
            cuts.push_back(contact.point);
        }
    }
}

PartClass classAfter(const ExactPoint& cut, const HalfPiece& along,
                     std::span<const LoopPieces> loops) {
    const std::array references{along};
    const auto sectors = sectorStates(cut, loops, references);
    const auto group = sectors.order.groupOf(sectors.loopHalfPieces());
    const auto previous = (group + sectors.order.groupCount() - 1) % sectors.order.groupCount();
    // Sweeping counterclockwise past the forward half piece enters its left
    // side. Each side keeps that parity until the next cut, including when
    // several loop pieces coincide with the host.
    const bool left = sectors.inside.at(group);
    const bool right = sectors.inside.at(previous);
    if (left == right) {
        return left ? PartClass::inside : PartClass::outside;
    }
    return left ? PartClass::boundaryLeft : PartClass::boundaryRight;
}

}  // namespace

Result<LoopPieces> checkedLoopPieces(const Loop& loop, std::size_t input) {
    LoopPieces result;
    if (const auto* circle = std::get_if<Circle>(&loop)) {
        if (auto problem = appendPiece(result, *circle, 0, input)) {
            return std::move(*problem);
        }
    } else {
        auto closed = std::get<Polyline>(loop);
        closed.closed = true;
        const auto segments = pieces(closed);
        for (std::size_t i = 0; i < segments.size(); ++i) {
            const auto& piece = segments[i];
            if (auto problem = appendPiece(result, piece, i, input)) {
                const auto vertex =
                    validPoint(piece.start) && std::isfinite(piece.bulge) && !validPoint(piece.end)
                        ? (i + 1) % closed.vertices.size()
                        : i;
                problem->message = "vertex " + std::to_string(vertex) + ": " + problem->message;
                return std::move(*problem);
            }
        }
    }
    if (result.pieces.empty()) {
        return degenerateDiagnosis(input);
    }
    return result;
}

RegionParts regionParts(const CurveMeaning& host, Extension extension,
                        std::span<const LoopPieces> loops) {
    RegionParts result;
    const auto* arc = std::get_if<ArcMeaning>(&host.support);
    if (arc != nullptr) {
        extension = Extension::none;
        result.closed = fullCircle(host);
    }
    if (extension == Extension::none) {
        result.cuts.push_back(startPoint(host));
        if (!result.closed) {
            result.cuts.push_back(endPoint(host));
        }
    }
    const auto carrier = carrierOf(host);
    for (const auto& loop : loops) {
        for (const auto& piece : loop.pieces) {
            addCuts(result.cuts, host, carrier, extension, piece);
        }
    }
    std::ranges::sort(result.cuts, [&host](const ExactPoint& a, const ExactPoint& b) {
        return compareAlong(host, a, b) == Sign::negative;
    });
    result.cuts.erase(std::unique(result.cuts.begin(), result.cuts.end(), samePoint),
                      result.cuts.end());
    if (result.closed) {
        result.cuts.push_back(startPoint(host));
    }
    const HalfPiece along{carrier, arc != nullptr && arc->turn == Turn::clockwise
                                       ? Heading::backward
                                       : Heading::forward};
    for (std::size_t i = 1; i < result.cuts.size(); ++i) {
        result.classes.push_back(classAfter(result.cuts[i - 1], along, loops));
    }
    return result;
}

}  // namespace solid_space::geom2d::detail
