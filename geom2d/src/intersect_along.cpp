// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// intersectAlong of intersection.hpp (stage 0.2, package P11; ADR 0010, point
// 6).
//
// Each cutter is intersected with the host by intersect() itself, so each
// cutter alone gives the points and the stretches of the pair call with the
// same bits by construction. What intersectAlong adds is the merge: one exact
// order along the host for the points of all the cutters, and the points of
// different cutters that are the same exact point gathered in one Cut.
//
// The merge needs the exact points, which intersect() rounds. They are found
// again here, by the same exact rules as intersection.cpp, without touching
// the rounded results:
// - a point with Contact::sameCurve comes from two curves on one carrier that
//   follow on from each other. Neither covers the whole carrier (or the shared
//   part would be a stretch), so the point is an end of the host: its start
//   when its location on the host is atStart, its end when it is atEnd;
// - every other point is a meeting of the two carriers (carriers.hpp, meet),
//   kept when it lies on the part of both curves that takes part and within
//   the domain, and listed in the exact order along the host: the same filter
//   and the same order as intersection.cpp, so the k-th point kept is the k-th
//   point of the pair result;
// - a stretch starts at the start of the host when it is whole or when its
//   start is atStart on the host; otherwise at an end of the cutter, the one
//   given by its location on the cutter (atStart or atEnd).
//
// Every order and every coincidence below is decided by compareAlong()
// (positions.hpp), which is exact for any two radicands: never by stations or
// rounded points (ADR 0010, point 4). Stations of equal points are equal bits
// (they depend only on the exact point and the curve), so the cuts and their
// stations are both in order.

#include "carriers.hpp"
#include "curve_meaning.hpp"
#include "positions.hpp"
#include "solid_space/geom2d/intersection.hpp"
#include "solid_space/robust/predicates.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

namespace solid_space::geom2d {
namespace {

using detail::CurveMeaning;
using detail::ExactPoint;
using robust::Sign;

// A point of one cutter on the host, with its exact value.
struct CutterPoint {
    ExactPoint exact;
    std::size_t cutter = 0;
    IntersectionPoint point;
};

// A stretch of one cutter, with the exact value of its start.
struct CutterStretch {
    ExactPoint start;
    CutterOverlap overlap;
};

// The exact ends of the host where the pair result has a sameCurve point.
ExactPoint sharedEnd(const CurveMeaning& host, const IntersectionPoint& point) {
    if (point.onFirst.location == Location::atStart) {
        return detail::startPoint(host);
    }
    assert(point.onFirst.location == Location::atEnd);
    return detail::endPoint(host);
}

// The exact meetings of the two carriers that intersect() lists, in its order.
std::vector<ExactPoint> carrierPoints(const CurveMeaning& host, Extension hostExtension,
                                      const CurveMeaning& cutter, Extension cutterExtension) {
    std::vector<ExactPoint> kept;
    const auto meeting = detail::meet(detail::carrierOf(host), detail::carrierOf(cutter));
    for (const auto& found : meeting.found()) {
        if (detail::takesPart(detail::locate(host, found.point), hostExtension) &&
            detail::takesPart(detail::locate(cutter, found.point), cutterExtension) &&
            !detail::beyondDomain(found.point)) {
            kept.push_back(found.point);
        }
    }
    std::sort(kept.begin(), kept.end(), [&host](const ExactPoint& a, const ExactPoint& b) {
        return detail::compareAlong(host, a, b) == Sign::negative;
    });
    return kept;
}

// The exact start of a stretch of the pair result.
ExactPoint stretchStart(const CurveMeaning& host, const CurveMeaning& cutter,
                        const Overlap& overlap) {
    if (overlap.whole || overlap.from.onFirst.location == Location::atStart) {
        return detail::startPoint(host);
    }
    if (overlap.from.onSecond.location == Location::atStart) {
        return detail::startPoint(cutter);
    }
    assert(overlap.from.onSecond.location == Location::atEnd);
    return detail::endPoint(cutter);
}

// Adds the points and the stretches of one cutter, with their exact values.
void addCutter(const CurveMeaning& host, Extension hostExtension, const CurveMeaning& cutter,
               Extension cutterExtension, std::size_t index, const PairIntersections& pair,
               std::vector<CutterPoint>& points, std::vector<CutterStretch>& stretches) {
    // A pair has either sameCurve points only (one carrier) or none.
    const bool shared = std::any_of(
        pair.points.begin(), pair.points.end(),
        [](const IntersectionPoint& point) { return point.contact == Contact::sameCurve; });
    std::vector<ExactPoint> exact;
    if (shared) {
        for (const auto& point : pair.points) {
            exact.push_back(sharedEnd(host, point));
        }
    } else if (!pair.points.empty()) {
        exact = carrierPoints(host, hostExtension, cutter, cutterExtension);
    }
    assert(exact.size() == pair.points.size());
    for (std::size_t k = 0; k < pair.points.size() && k < exact.size(); ++k) {
        points.push_back(CutterPoint{std::move(exact[k]), index, pair.points[k]});
    }
    for (const auto& overlap : pair.overlaps) {
        auto start = stretchStart(host, cutter, overlap);
        stretches.push_back(CutterStretch{std::move(start), CutterOverlap{index, overlap}});
    }
}

// The points of all the cutters, in the exact order along the host, the
// exactly equal ones gathered in one Cut.
std::vector<Cut> mergePoints(const CurveMeaning& host, std::vector<CutterPoint> points) {
    // Stable: equal points stay by increasing cutter index, the order in which
    // the cutters were added (one cutter has no two equal points).
    std::stable_sort(points.begin(), points.end(),
                     [&host](const CutterPoint& a, const CutterPoint& b) {
                         return detail::compareAlong(host, a.exact, b.exact) == Sign::negative;
                     });
    std::vector<Cut> cuts;
    const ExactPoint* previous = nullptr;
    for (const auto& entry : points) {
        if (previous == nullptr ||
            detail::compareAlong(host, *previous, entry.exact) != Sign::zero) {
            // The position on the host and the rounded point depend only on the
            // exact point: those of the first cutter serve every cutter here.
            cuts.push_back(Cut{entry.point.point, entry.point.onFirst, {}});
        }
        cuts.back().cutters.push_back(
            CutterContact{entry.cutter, entry.point.contact, entry.point.onSecond});
        previous = &entry.exact;
    }
    return cuts;
}

// The stretches of all the cutters, by start along the host, then by index.
std::vector<CutterOverlap> mergeStretches(const CurveMeaning& host,
                                          std::vector<CutterStretch> stretches) {
    // Stable: the stretches were added by increasing cutter index, and those
    // of one cutter in the order of their start.
    std::stable_sort(stretches.begin(), stretches.end(),
                     [&host](const CutterStretch& a, const CutterStretch& b) {
                         return detail::compareAlong(host, a.start, b.start) == Sign::negative;
                     });
    std::vector<CutterOverlap> overlaps;
    overlaps.reserve(stretches.size());
    for (const auto& stretch : stretches) {
        overlaps.push_back(stretch.overlap);
    }
    return overlaps;
}

}  // namespace

Result<HostIntersections> intersectAlong(const Curve& host, Extension hostExtension,
                                         std::span<const Curve> cutters,
                                         Extension cutterExtension) {
    auto hostMeaning = detail::checkedMeaning(host, 0);
    if (!hostMeaning.hasValue()) {
        return hostMeaning.diagnosis();
    }
    const auto& hostCurve = hostMeaning.value();
    HostIntersections result;
    std::vector<CutterPoint> points;
    std::vector<CutterStretch> stretches;
    for (std::size_t index = 0; index < cutters.size(); ++index) {
        const auto& cutter = cutters[index];
        auto cutterMeaning = detail::checkedMeaning(cutter, index + 1);
        if (!cutterMeaning.hasValue()) {
            result.skipped.push_back(SkippedCutter{index, cutterMeaning.diagnosis()});
            continue;
        }
        // Both curves passed the checks of intersect(), which therefore gives a
        // value. Should it not, the cutter is listed as skipped, never dropped.
        const auto pair = intersect(host, hostExtension, cutter, cutterExtension);
        assert(pair.hasValue());
        if (!pair.hasValue()) {
            auto diagnosis = pair.diagnosis();
            diagnosis.input = index + 1;
            result.skipped.push_back(SkippedCutter{index, std::move(diagnosis)});
            continue;
        }
        const auto& found = pair.value();
        result.omittedBeyondDomain += found.omittedBeyondDomain;
        addCutter(hostCurve, hostExtension, cutterMeaning.value(), cutterExtension, index, found,
                  points, stretches);
    }
    result.cuts = mergePoints(hostCurve, std::move(points));
    result.overlaps = mergeStretches(hostCurve, std::move(stretches));
    return result;
}

}  // namespace solid_space::geom2d
