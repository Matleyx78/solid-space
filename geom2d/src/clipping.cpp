// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "solid_space/geom2d/clipping.hpp"

#include "positions.hpp"
#include "region_parts.hpp"

#include <cmath>
#include <cstddef>
#include <utility>
#include <variant>

namespace solid_space::geom2d {
namespace {

CurvePosition stretchPosition(const detail::CurveMeaning& host, const detail::ExactPoint& point) {
    const auto* piece = std::get_if<PolylineSegment>(&host.curve);
    // For |bulge| <= 2^-26 the total turn is at most 2^-24. The
    // chord projection then differs from arc length by at most
    // (1 - cos(2^-25)) times the arc length, within the station error.
    // It also avoids converting a huge radius (or a tiny angle) to double.
    constexpr double kShallowBulge = 0x1p-26;
    if (piece != nullptr && std::fabs(piece->bulge) <= kShallowBulge) {
        const auto chord = detail::meaningOf(Segment{piece->start, piece->end});
        return {detail::locate(host, point), detail::station(chord, point)};
    }
    return detail::positionOn(host, point);
}

StretchEnd stretchEnd(const detail::CurveMeaning& host, const detail::RegionParts& parts,
                      std::size_t cut) {
    const auto& point = parts.cuts[cut];
    if (parts.closed) {
        if (cut == 0) {
            return {endpoints(host.curve).value().start, {Location::atStart, 0.0}};
        }
        if (cut == parts.classes.size()) {
            return {endpoints(host.curve).value().start,
                    {Location::atEnd, length(host.curve).value()}};
        }
    }
    const auto position = stretchPosition(host, point);
    if (position.location == Location::atStart) {
        return {endpoints(host.curve).value().start, position};
    }
    if (position.location == Location::atEnd) {
        return {endpoints(host.curve).value().end, position};
    }
    return {detail::nearestPoint(point), position};
}

ClippedHost clipHost(const detail::CurveMeaning& host, Extension extension,
                     std::span<const detail::LoopPieces> loops) {
    using detail::PartClass;
    const auto parts = detail::regionParts(host, extension, loops);
    ClippedHost clipped;
    for (std::size_t from = 0; from < parts.classes.size();) {
        const auto kind = parts.classes[from];
        std::size_t to = from + 1;
        while (to < parts.classes.size() && parts.classes[to] == kind) {
            ++to;
        }
        if (kind == PartClass::inside) {
            clipped.inside.push_back({stretchEnd(host, parts, from), stretchEnd(host, parts, to)});
        } else if (kind != PartClass::outside) {
            const auto side = kind == PartClass::boundaryLeft ? Side::left : Side::right;
            clipped.boundary.push_back(
                {stretchEnd(host, parts, from), stretchEnd(host, parts, to), side});
        }
        from = to;
    }
    return clipped;
}

}  // namespace

RegionClipping clipToRegion(std::span<const Curve> hosts, Extension hostExtension,
                            std::span<const Loop> region) {
    RegionClipping result;
    result.hosts.resize(hosts.size());
    std::vector<detail::LoopPieces> loops;
    loops.reserve(region.size());
    for (std::size_t i = 0; i < region.size(); ++i) {
        auto checked = detail::checkedLoopPieces(region[i], i);
        if (checked.hasValue()) {
            loops.push_back(std::move(checked).value());
        } else {
            result.skippedLoops.push_back({i, checked.diagnosis()});
        }
    }
    for (std::size_t i = 0; i < hosts.size(); ++i) {
        const auto checked = detail::checkedMeaning(hosts[i], i);
        if (!checked.hasValue()) {
            result.skippedHosts.push_back({i, checked.diagnosis()});
        } else if (!loops.empty()) {
            result.hosts[i] = clipHost(checked.value(), hostExtension, loops);
        }
    }
    return result;
}

}  // namespace solid_space::geom2d
