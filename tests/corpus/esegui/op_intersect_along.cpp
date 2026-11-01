// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// The cases of intersectAlong() (stage 0.2, package P11; references
// [0.2-R11]). Every case also calls intersect for each cutter alone and fails
// unless that cutter's points and stretches in the result have the same bits
// as the pair call (intersection.hpp, intersectAlong: "Each cutter alone gives
// the same points"), and unless a cutter is skipped exactly when the pair call
// gives a diagnosis about it.

#include "operations.hpp"
#include "plane_data.hpp"
#include "solid_space/geom2d/intersection.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace solid_space::corpus {
namespace {

using geom2d::Curve;
using geom2d::CurvePosition;
using geom2d::Extension;
using geom2d::HostIntersections;
using geom2d::IntersectionPoint;
using geom2d::Overlap;
using geom2d::OverlapEnd;

bool sameBits(double a, double b) {
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

bool sameBits(robust::Point2 a, robust::Point2 b) {
    return sameBits(a.x, b.x) && sameBits(a.y, b.y);
}

bool sameBits(const CurvePosition& a, const CurvePosition& b) {
    return a.location == b.location && sameBits(a.station, b.station);
}

bool sameBits(const OverlapEnd& a, const OverlapEnd& b) {
    return sameBits(a.point, b.point) && sameBits(a.onFirst, b.onFirst) &&
           sameBits(a.onSecond, b.onSecond);
}

bool sameBits(const Overlap& a, const Overlap& b) {
    return a.whole == b.whole && sameBits(a.from, b.from) && sameBits(a.to, b.to);
}

[[noreturn]] void differs(std::size_t cutter, const std::string& what) {
    throw std::runtime_error("intersectAlong differs from intersect for cutter " +
                             std::to_string(cutter) + ": " + what);
}

// The points of one cutter, in the order of the cuts.
std::vector<IntersectionPoint> pointsOf(const HostIntersections& result, std::size_t cutter) {
    std::vector<IntersectionPoint> points;
    for (const auto& cut : result.cuts) {
        for (const auto& contact : cut.cutters) {
            if (contact.cutter == cutter) {
                points.push_back(
                    IntersectionPoint{cut.point, contact.contact, cut.onHost, contact.onCutter});
            }
        }
    }
    return points;
}

std::vector<Overlap> overlapsOf(const HostIntersections& result, std::size_t cutter) {
    std::vector<Overlap> overlaps;
    for (const auto& overlap : result.overlaps) {
        if (overlap.cutter == cutter) {
            overlaps.push_back(overlap.overlap);
        }
    }
    return overlaps;
}

const geom2d::SkippedCutter* skippedOf(const HostIntersections& result, std::size_t cutter) {
    for (const auto& skipped : result.skipped) {
        if (skipped.cutter == cutter) {
            return &skipped;
        }
    }
    return nullptr;
}

void checkCutter(const HostIntersections& result, std::size_t cutter,
                 const geom2d::Result<geom2d::PairIntersections>& pair) {
    const auto* skipped = skippedOf(result, cutter);
    if (!pair.hasValue()) {
        if (skipped == nullptr || skipped->diagnosis.code != pair.diagnosis().code ||
            skipped->diagnosis.input != cutter + 1) {
            differs(cutter, "the pair call gives a diagnosis, the cutter is not skipped as such");
        }
        return;
    }
    if (skipped != nullptr) {
        differs(cutter, "skipped, but the pair call gives a value");
    }
    const auto points = pointsOf(result, cutter);
    const auto& expected = pair.value().points;
    if (points.size() != expected.size()) {
        differs(cutter, "a different number of points");
    }
    for (std::size_t k = 0; k < points.size(); ++k) {
        const auto& a = points[k];
        const auto& b = expected[k];
        if (!sameBits(a.point, b.point) || a.contact != b.contact ||
            !sameBits(a.onFirst, b.onFirst) || !sameBits(a.onSecond, b.onSecond)) {
            differs(cutter, "point " + std::to_string(k));
        }
    }
    const auto overlaps = overlapsOf(result, cutter);
    const auto& expectedOverlaps = pair.value().overlaps;
    if (overlaps.size() != expectedOverlaps.size()) {
        differs(cutter, "a different number of overlaps");
    }
    for (std::size_t k = 0; k < overlaps.size(); ++k) {
        if (!sameBits(overlaps[k], expectedOverlaps[k])) {
            differs(cutter, "overlap " + std::to_string(k));
        }
    }
}

// Each cutter alone, and the count of the points beyond the domain.
void checkPairs(const Curve& host, Extension hostPart, const std::vector<Curve>& cutters,
                Extension cutterPart, const HostIntersections& result) {
    std::size_t omitted = 0;
    for (std::size_t cutter = 0; cutter < cutters.size(); ++cutter) {
        const auto pair = geom2d::intersect(host, hostPart, cutters[cutter], cutterPart);
        checkCutter(result, cutter, pair);
        if (pair.hasValue()) {
            omitted += pair.value().omittedBeyondDomain;
        }
    }
    if (omitted != result.omittedBeyondDomain) {
        throw std::runtime_error("intersectAlong: omittedBeyondDomain is not the sum");
    }
}

Value index(std::size_t value) {
    return Value::integer(static_cast<std::int64_t>(value));
}

Value writeEnd(const OverlapEnd& end) {
    return Value::group({{"punto", writePoint(end.point)},
                         {"sulla_prima", writePosition(end.onFirst)},
                         {"sulla_seconda", writePosition(end.onSecond)}});
}

Value writeResult(const HostIntersections& result) {
    Value::List cuts;
    for (const auto& cut : result.cuts) {
        Value::List contacts;
        for (const auto& contact : cut.cutters) {
            contacts.push_back(Value::group({{"indice", index(contact.cutter)},
                                             {"contatto", writeName(contact.contact)},
                                             {"sul_tagliente", writePosition(contact.onCutter)}}));
        }
        cuts.push_back(Value::group({{"punto", writePoint(cut.point)},
                                     {"sull_ospite", writePosition(cut.onHost)},
                                     {"taglienti", Value::list(std::move(contacts))}}));
    }
    Value::List overlaps;
    for (const auto& overlap : result.overlaps) {
        overlaps.push_back(Value::group({{"tagliente", index(overlap.cutter)},
                                         {"intera", Value::boolean(overlap.overlap.whole)},
                                         {"da", writeEnd(overlap.overlap.from)},
                                         {"a", writeEnd(overlap.overlap.to)}}));
    }
    Value::List skipped;
    for (const auto& cutter : result.skipped) {
        // writeDiagnosis wraps the fields in "diagnosi", as a whole result.
        auto diagnosis = writeDiagnosis(cutter.diagnosis);
        skipped.push_back(Value::group(
            {{"tagliente", index(cutter.cutter)}, {"diagnosi", *diagnosis.find("diagnosi")}}));
    }
    return Value::group({{"tagli", Value::list(std::move(cuts))},
                         {"sovrapposizioni", Value::list(std::move(overlaps))},
                         {"scartati", Value::list(std::move(skipped))},
                         {"omessi_fuori_dominio", index(result.omittedBeyondDomain)}});
}

std::vector<Curve> readCutters(const Value& value) {
    if (value.kind() != Value::Kind::list) {
        throw std::runtime_error("taglienti: expected a list of curves");
    }
    std::vector<Curve> cutters;
    const auto& items = value.asList();
    cutters.reserve(items.size());
    for (std::size_t i = 0; i < items.size(); ++i) {
        cutters.push_back(readCurve(items[i], "taglienti[" + std::to_string(i) + "]"));
    }
    return cutters;
}

Value intersectAlongOperation(const Value& input) {
    const auto host = readCurve(field(input, "curva"), "curva");
    const auto hostPart = readExtension(field(input, "prolunga"), "prolunga");
    const auto cutters = readCutters(field(input, "taglienti"));
    const auto cutterPart = readExtension(field(input, "prolunga_taglienti"), "prolunga_taglienti");
    const auto result = geom2d::intersectAlong(host, hostPart, cutters, cutterPart);
    if (!result.hasValue()) {
        return writeDiagnosis(result.diagnosis());
    }
    checkPairs(host, hostPart, cutters, cutterPart, result.value());
    return writeResult(result.value());
}

const Registration kIntersectAlong("intersectAlong", intersectAlongOperation);

}  // namespace
}  // namespace solid_space::corpus
