// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// The cases of the two intersect() of ellipse.hpp (stage 0.3, package PS7;
// references [0.3-RS7a], [0.3-RS7b] and [0.3-RS7c]). With two ellipses every
// case also calls intersect with the arguments swapped and fails unless it
// gives the same decisions, with onFirst and onSecond swapped (ellipse.hpp,
// the second intersect: "Symmetric in the decisions"), or the diagnosis of
// the same bad ellipse with the input swapped. The rounded points need not
// have the same bits: each is within kRelativePointError of the same exact
// point, so the two are within twice that error.

#include "ellipse_data.hpp"
#include "operations.hpp"
#include "plane_data.hpp"
#include "solid_space/geom2d/ellipse.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace solid_space::corpus {
namespace {

using geom2d::EllipseIntersectionPoint;
using geom2d::EllipseIntersections;
using geom2d::EllipseOverlap;
using geom2d::EllipseOverlapEnd;

[[noreturn]] void asymmetric(const std::string& what) {
    throw std::runtime_error("intersect of two ellipses is not symmetric: " + what);
}

// Two rounded points of the same exact point: each coordinate within
// kRelativePointError * max(M, |p|) of it.
bool close(robust::Point2 a, robust::Point2 b, double magnitude) {
    const auto near = [magnitude](double u, double v) {
        const double scale = std::max({magnitude, std::fabs(u), std::fabs(v)});
        return std::fabs(u - v) <= 2.0 * geom2d::kRelativePointError * scale;
    };
    return near(a.x, b.x) && near(a.y, b.y);
}

bool sameEndSwapped(const EllipseOverlapEnd& end, const EllipseOverlapEnd& other,
                    double magnitude) {
    return close(end.point, other.point, magnitude) && end.onFirst == other.onSecond &&
           end.onSecond == other.onFirst;
}

// A point of `swapped`, not matched yet, with the same contact, the positions
// swapped and a rounded point of the same exact point.
void checkPoint(const EllipseIntersectionPoint& point, const EllipseIntersections& swapped,
                double magnitude, std::vector<bool>& used) {
    for (std::size_t i = 0; i < swapped.points.size(); ++i) {
        const auto& other = swapped.points[i];
        if (!used[i] && point.contact == other.contact && point.onFirst == other.onSecond &&
            point.onSecond == other.onFirst && close(point.point, other.point, magnitude)) {
            used[i] = true;
            return;
        }
    }
    asymmetric("a point is missing with the arguments swapped");
}

// A stretch that is not whole has the same two ends, in either order (the
// order follows the first ellipse).
void checkOverlap(const EllipseOverlap& overlap, const EllipseIntersections& swapped,
                  double magnitude) {
    for (const auto& other : swapped.overlaps) {
        if (overlap.whole || other.whole) {
            if (overlap.whole && other.whole) {
                return;
            }
            continue;
        }
        if ((sameEndSwapped(overlap.from, other.from, magnitude) &&
             sameEndSwapped(overlap.to, other.to, magnitude)) ||
            (sameEndSwapped(overlap.from, other.to, magnitude) &&
             sameEndSwapped(overlap.to, other.from, magnitude))) {
            return;
        }
    }
    asymmetric("an overlap is missing with the arguments swapped");
}

// The diagnosis of one ellipse alone, if any: endpoints() checks it as
// intersect does (ellipse.hpp, "Checking the data"), as input 0.
std::optional<geom2d::Diagnosis> diagnosisOf(const geom2d::Ellipse& ellipse) {
    const auto ends = geom2d::endpoints(ellipse);
    if (ends.hasValue()) {
        return std::nullopt;
    }
    return ends.diagnosis();
}

// Whether a call reports the ellipse it is given first (leading), if bad, as
// input 0, and otherwise the one given second (trailing) as input 1.
bool reportsFirstBad(const geom2d::Diagnosis& diagnosis,
                     const std::optional<geom2d::Diagnosis>& leading,
                     const std::optional<geom2d::Diagnosis>& trailing) {
    if (leading.has_value()) {
        return diagnosis.input == 0 && diagnosis.code == leading->code;
    }
    return trailing.has_value() && diagnosis.input == 1 && diagnosis.code == trailing->code;
}

// With only one bad ellipse both calls report it with the same code, as
// input 1 in one call and as input 0 in the other; with two bad ellipses
// each call reports the one it is given first.
void checkDiagnoses(const geom2d::Diagnosis& direct, const geom2d::Diagnosis& swapped,
                    const geom2d::Ellipse& first, const geom2d::Ellipse& second) {
    const auto ofEllipse = diagnosisOf(first);
    const auto ofOther = diagnosisOf(second);
    if (!reportsFirstBad(direct, ofEllipse, ofOther) ||
        !reportsFirstBad(swapped, ofOther, ofEllipse)) {
        asymmetric("the diagnoses do not report the same bad ellipse");
    }
}

void checkSymmetry(const geom2d::Result<EllipseIntersections>& direct,
                   const geom2d::Result<EllipseIntersections>& swapped,
                   const geom2d::Ellipse& first, const geom2d::Ellipse& second, double magnitude) {
    if (direct.hasValue() != swapped.hasValue()) {
        asymmetric("only one of the two calls gives a diagnosis");
    }
    if (!direct.hasValue()) {
        checkDiagnoses(direct.diagnosis(), swapped.diagnosis(), first, second);
        return;
    }
    const auto& a = direct.value();
    const auto& b = swapped.value();
    if (a.points.size() != b.points.size() || a.overlaps.size() != b.overlaps.size() ||
        a.omittedBeyondDomain != b.omittedBeyondDomain) {
        asymmetric("the counts differ");
    }
    std::vector<bool> used(b.points.size(), false);
    for (const auto& point : a.points) {
        checkPoint(point, b, magnitude, used);
    }
    for (const auto& overlap : a.overlaps) {
        checkOverlap(overlap, b, magnitude);
    }
}

// The magnitude M of the two ellipses, or zero when one is not valid (then
// both calls give a diagnosis and no point is compared).
double magnitudeOf(const geom2d::Ellipse& first, const geom2d::Ellipse& second) {
    const auto a = geom2d::magnitude(first);
    const auto b = geom2d::magnitude(second);
    if (!a.hasValue() || !b.hasValue()) {
        return 0.0;
    }
    return std::max(a.value(), b.value());
}

Value writeEnd(const EllipseOverlapEnd& end) {
    return Value::group({{"punto", writePoint(end.point)},
                         {"sulla_prima", writeEllipseLocation(end.onFirst)},
                         {"sulla_seconda", writeEllipseLocation(end.onSecond)}});
}

Value writeResult(const EllipseIntersections& result) {
    Value::List points;
    for (const auto& point : result.points) {
        points.push_back(Value::group({{"punto", writePoint(point.point)},
                                       {"contatto", writeName(point.contact)},
                                       {"sulla_prima", writeEllipseLocation(point.onFirst)},
                                       {"sulla_seconda", writeEllipseLocation(point.onSecond)}}));
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

Value ellipseIntersectOperation(const Value& input) {
    const auto ellipse = readEllipse(field(input, "ellisse"));
    const auto ellipsePart = readExtension(field(input, "prolunga_ellisse"), "prolunga_ellisse");
    const auto otherPart = readExtension(field(input, "prolunga_seconda"), "prolunga_seconda");
    geom2d::Result<EllipseIntersections> direct = EllipseIntersections{};
    if (const auto* value = input.find("seconda_ellisse")) {
        // Named apart from the parameters of intersect, which the swapped
        // call crosses on purpose.
        const auto other = readEllipse(*value, "seconda_ellisse");
        direct = geom2d::intersect(ellipse, ellipsePart, other, otherPart);
        const auto swapped = geom2d::intersect(other, otherPart, ellipse, ellipsePart);
        checkSymmetry(direct, swapped, ellipse, other, magnitudeOf(ellipse, other));
    } else {
        const auto curve = readCurve(field(input, "curva"), "curva");
        direct = geom2d::intersect(ellipse, ellipsePart, curve, otherPart);
    }
    if (!direct.hasValue()) {
        return writeDiagnosis(direct.diagnosis());
    }
    return writeResult(direct.value());
}

const Registration kEllipseIntersect("ellipseIntersect", ellipseIntersectOperation);

}  // namespace
}  // namespace solid_space::corpus
