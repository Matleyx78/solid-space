// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// The cases of intersect() (stage 0.2, package P10; references [0.2-R10a] and
// [0.2-R10b]). Every case also calls intersect with the arguments swapped and
// fails unless it gives the same points with the same bits, with the roles of
// the two curves swapped (intersection.hpp, intersect: "Symmetric").

#include "operations.hpp"
#include "plane_data.hpp"
#include "solid_space/geom2d/intersection.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

namespace solid_space::corpus {
namespace {

using geom2d::CurvePosition;
using geom2d::IntersectionPoint;
using geom2d::Overlap;
using geom2d::OverlapEnd;
using geom2d::PairIntersections;

bool sameBits(double a, double b) {
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

bool sameBits(robust::Point2 a, robust::Point2 b) {
    return sameBits(a.x, b.x) && sameBits(a.y, b.y);
}

bool sameBits(const CurvePosition& a, const CurvePosition& b) {
    return a.location == b.location && sameBits(a.station, b.station);
}

[[noreturn]] void asymmetric(const std::string& what) {
    throw std::runtime_error("intersect is not symmetric: " + what);
}

bool sameEndSwapped(const OverlapEnd& end, const OverlapEnd& other) {
    return sameBits(end.point, other.point) && sameBits(end.onFirst, other.onSecond) &&
           sameBits(end.onSecond, other.onFirst);
}

// A point of `swapped` with the same bits, the same contact and the positions
// swapped. Two different exact points may round to the same double, so every
// point with those bits is a candidate.
void checkPoint(const IntersectionPoint& point, const PairIntersections& swapped) {
    for (const auto& other : swapped.points) {
        if (sameBits(point.point, other.point) && point.contact == other.contact &&
            sameBits(point.onFirst, other.onSecond) && sameBits(point.onSecond, other.onFirst)) {
            return;
        }
    }
    asymmetric("a point is missing with the arguments swapped");
}

// A stretch that is not whole has the same two ends, in either order (the
// order follows the first curve).
void checkOverlap(const Overlap& overlap, const PairIntersections& swapped) {
    for (const auto& other : swapped.overlaps) {
        if (overlap.whole) {
            if (other.whole) {
                return;
            }
            continue;
        }
        if (other.whole) {
            continue;
        }
        if ((sameEndSwapped(overlap.from, other.from) && sameEndSwapped(overlap.to, other.to)) ||
            (sameEndSwapped(overlap.from, other.to) && sameEndSwapped(overlap.to, other.from))) {
            return;
        }
    }
    asymmetric("an overlap is missing with the arguments swapped");
}

void checkSymmetry(const geom2d::Result<PairIntersections>& direct,
                   const geom2d::Result<PairIntersections>& swapped) {
    if (direct.hasValue() != swapped.hasValue()) {
        asymmetric("only one of the two calls gives a diagnosis");
    }
    if (!direct.hasValue()) {
        // The same curve reported, as the other input, when only one is bad.
        const auto& a = direct.diagnosis();
        const auto& b = swapped.diagnosis();
        if (a.input + b.input == 1 && a.code != b.code) {
            asymmetric("the diagnoses differ");
        }
        return;
    }
    const auto& a = direct.value();
    const auto& b = swapped.value();
    if (a.points.size() != b.points.size() || a.overlaps.size() != b.overlaps.size() ||
        a.omittedBeyondDomain != b.omittedBeyondDomain) {
        asymmetric("the counts differ");
    }
    for (const auto& point : a.points) {
        checkPoint(point, b);
    }
    for (const auto& overlap : a.overlaps) {
        checkOverlap(overlap, b);
    }
}

Value writeEnd(const OverlapEnd& end) {
    return Value::group({{"punto", writePoint(end.point)},
                         {"sulla_prima", writePosition(end.onFirst)},
                         {"sulla_seconda", writePosition(end.onSecond)}});
}

Value writeResult(const PairIntersections& result) {
    Value::List points;
    for (const auto& point : result.points) {
        points.push_back(Value::group({{"punto", writePoint(point.point)},
                                       {"contatto", writeName(point.contact)},
                                       {"sulla_prima", writePosition(point.onFirst)},
                                       {"sulla_seconda", writePosition(point.onSecond)}}));
    }
    Value::List overlaps;
    for (const auto& overlap : result.overlaps) {
        overlaps.push_back(Value::group({{"intera", Value::boolean(overlap.whole)},
                                         {"da", writeEnd(overlap.from)},
                                         {"a", writeEnd(overlap.to)}}));
    }
    return Value::group({{"punti", Value::list(std::move(points))},
                         {"sovrapposizioni", Value::list(std::move(overlaps))},
                         {"omessi_fuori_dominio",
                          Value::integer(static_cast<std::int64_t>(result.omittedBeyondDomain))}});
}

Value intersectOperation(const Value& input) {
    // Named apart from the parameters of intersect, which the swapped call
    // crosses on purpose.
    const auto left = readCurve(field(input, "prima"), "prima");
    const auto leftPart = readExtension(field(input, "prolunga_prima"), "prolunga_prima");
    const auto right = readCurve(field(input, "seconda"), "seconda");
    const auto rightPart = readExtension(field(input, "prolunga_seconda"), "prolunga_seconda");
    const auto direct = geom2d::intersect(left, leftPart, right, rightPart);
    const auto swapped = geom2d::intersect(right, rightPart, left, leftPart);
    checkSymmetry(direct, swapped);
    if (!direct.hasValue()) {
        return writeDiagnosis(direct.diagnosis());
    }
    return writeResult(direct.value());
}

const Registration kIntersect("intersect", intersectOperation);

}  // namespace
}  // namespace solid_space::corpus
