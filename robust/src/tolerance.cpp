// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "solid_space/robust/tolerance.hpp"

#include "exact_sum.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

namespace solid_space::robust {
namespace {

[[maybe_unused]] bool inDomain(double value) noexcept {
    // False for NaN as well as for infinities and values beyond the domain.
    return std::fabs(value) <= kMaxCoordinate;
}

[[maybe_unused]] bool isMagnitude(double value) noexcept {
    return value >= 0.0 && value <= kMaxCoordinate;
}

Diagnosis invalidUnit(const char* reason) {
    return Diagnosis{DiagnosisCode::invalidValue, reason};
}

// The floating-point filter of coincident.
//
// It compares the computed squared distance D^ = fl(fl(dx^2) + fl(dy^2)), with
// dx = fl(bx - ax) and dy = fl(by - ay), with the computed E^ = fl(e * e),
// e = epsilon(M). With u = 2^-53 and eta = 2^-1075 (the error of a product in
// the subnormal range), and D, E the exact values:
//   D * (1 - u)^4 - 2 * eta <= D^ <= D * (1 + u)^4 + 3 * eta,
//   E * (1 - u) - eta <= E^ <= E * (1 + u) + eta,
// because a difference has relative error at most u and no absolute error, a
// product at most u relative or eta absolute, and a sum u relative.
// The thresholds are computed in doubles, with a relative margin of 2^-20 and
// an absolute one of 2^-1000:
//   high = fl(fl(E^ * (1 + 2^-20)) + 2^-1000)
//        >= E^ * (1 + 2^-21) + 2^-1001 > E * (1 + u)^4 + 3 * eta,
//   low  = fl(fl(E^ * (1 - 2^-20)) - 2^-1000)
//        <= E^ * (1 - 2^-21) - 2^-1001 < E * (1 - u)^4 - 2 * eta
//        (or it is not positive, and D^ < low never holds).
// So D^ > high proves D > E (not coincident), and D^ < low proves D < E
// (coincident). In between, the exact sum decides. Nothing overflows: the
// differences are at most 2^51, epsilon at most 2^50, their squares 2^102.
constexpr double kFilterAbove = 1.0 + 0x1p-20;
constexpr double kFilterBelow = 1.0 - 0x1p-20;
constexpr double kFilterAbsolute = 0x1p-1000;

}  // namespace

TolerancePolicy::TolerancePolicy(double unitInMeters, double floorLength) noexcept
    : m_unitInMeters(unitInMeters), m_floorLength(floorLength) {}

Result<TolerancePolicy, Diagnosis> TolerancePolicy::forUnit(double unitInMeters) {
    if (std::isnan(unitInMeters)) {
        return invalidUnit("the unit length is NaN");
    }
    if (std::isinf(unitInMeters)) {
        return invalidUnit("the unit length is infinite");
    }
    if (!(unitInMeters > 0.0)) {
        return invalidUnit("the unit length is not positive");
    }
    // One correctly rounded division: the same double on every platform.
    const double floorLength = kPhysicalFloorMeters / unitInMeters;
    if (!(floorLength > 0.0)) {
        return invalidUnit("the unit length is so large that the tolerance floor is zero");
    }
    if (!(floorLength <= kMaxCoordinate)) {
        return invalidUnit(
            "the unit length is so small that the tolerance floor is beyond kMaxCoordinate");
    }
    return TolerancePolicy(unitInMeters, floorLength);
}

double TolerancePolicy::unitInMeters() const noexcept {
    return m_unitInMeters;
}

double TolerancePolicy::floorLength() const noexcept {
    return m_floorLength;
}

double TolerancePolicy::epsilon(double magnitude) const noexcept {
    assert(isMagnitude(magnitude));
    // A product by a power of two, correctly rounded (exact unless it falls in
    // the subnormal range), and a maximum: the same double on every platform.
    return std::max(m_floorLength, kRelativeTolerance * magnitude);
}

bool TolerancePolicy::coincident(Point2 a, Point2 b) const noexcept {
    assert(inDomain(a.x) && inDomain(a.y) && inDomain(b.x) && inDomain(b.y));
    const double magnitude =
        std::max({std::fabs(a.x), std::fabs(a.y), std::fabs(b.x), std::fabs(b.y)});
    const double tolerance = epsilon(magnitude);

    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double distanceSquared = dx * dx + dy * dy;
    const double toleranceSquared = tolerance * tolerance;
    if (distanceSquared > toleranceSquared * kFilterAbove + kFilterAbsolute) {
        return false;
    }
    if (distanceSquared < toleranceSquared * kFilterBelow - kFilterAbsolute) {
        return true;
    }
    // The exact sign of |b - a|^2 - epsilon^2, developed into products of the
    // given doubles: bx^2 - 2 ax bx + ax^2 + by^2 - 2 ay by + ay^2 - e^2.
    exact::ExactSum sum;
    sum.add(b.x, b.x);
    sum.subtract(a.x, b.x);
    sum.subtract(a.x, b.x);
    sum.add(a.x, a.x);
    sum.add(b.y, b.y);
    sum.subtract(a.y, b.y);
    sum.subtract(a.y, b.y);
    sum.add(a.y, a.y);
    sum.subtract(tolerance, tolerance);
    return sum.sign() != Sign::positive;
}

bool TolerancePolicy::negligible(double length, double magnitude) const noexcept {
    assert(isMagnitude(length) && isMagnitude(magnitude));
    // Both are doubles: the comparison is exact.
    return length <= epsilon(magnitude);
}

}  // namespace solid_space::robust
