// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// The intervals of the floating-point filter (interval.hpp; stage 0.3
// corrections, package FS2).
//
// Notation: u = 2^-53 is the unit roundoff of round to nearest, fl(z) the
// double that an operation gives for its exact result z. Two facts about
// round to nearest are used throughout.
//
// Fact 1 (the error of a result). |fl(z) - z| <= u |fl(z)| + 2^-1075. When
// fl(z) is normal, in [2^k, 2^(k+1)), the error is at most half an ulp,
// 2^(k-53) = u 2^k <= u |fl(z)| (also when fl(z) = 2^k and z is below it,
// where the ulp is half as large); when fl(z) is subnormal or zero, it is at
// most half the smallest subnormal, 2^-1075. A sum or a difference whose
// result is subnormal is exact, so for them the first term alone is enough.
//
// Fact 2 (the computed bounds). For doubles x, y >= 0:
//     fl(x + y) >= (x + y)(1 - u),
//     fl(x y) >= x y (1 - u) - 2^-1075,    fl(x / y) >= (x / y)(1 - u) - 2^-1075,
// by Fact 1 (a relative error below u for a normal result, an absolute one of
// at most 2^-1075 for a subnormal one, none for a subnormal sum).
//
// Every radius below is first computed as a raw sum r of non-negative terms,
// for which Fact 2 gives r >= R (1 - u)^k - m 2^-1075 for the true bound R
// that the proof next to the operation derives, with k <= 24 and m <= 2^40;
// then finished() turns r into a radius that is at least R (proof there). An
// overflow anywhere makes the radius +infinity, and 0 * infinity makes it NaN:
// both are "nothing" for certainSign(), whose test fails for them.

#include "interval.hpp"

#include <atomic>
#include <cmath>
#include <limits>

namespace solid_space::geom2d::exact {
namespace {

using robust::Sign;

// The unit roundoff of round to nearest.
constexpr double kUnit = 0x1p-53;

// The absolute slack added to every radius: it covers the lost subnormal
// halves m 2^-1075 <= 2^40 2^-1075 = 2^-1035 of the raw bounds with room to
// spare, and keeps every finished radius normal.
constexpr double kSlack = 0x1p-1020;

// The factor that covers the relative roundings of a raw bound:
// 1 + 2^-48 = 1 + 32 u.
constexpr double kInflate = 1.0 + 0x1p-48;

// The smallest normal double: a quotient below it has an absolute rounding
// error that Fact 1 does not bound relatively.
constexpr double kSmallestNormal = 0x1p-1022;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

// The radius of a raw bound r >= R (1 - u)^k - m 2^-1075 (k <= 24,
// m <= 2^40): fl(fl(r + kSlack) * kInflate) >= R.
//
// Proof. s = fl(r + kSlack) >= (r + kSlack)(1 - u) by Fact 2 (both terms are
// non-negative). s >= kSlack (1 - u) is normal and so is s * kInflate, hence
// fl(s * kInflate) >= s kInflate (1 - u) >= (r + kSlack)(1 - u)^2 kInflate
//     >= (R (1 - u)^k + kSlack - m 2^-1075)(1 - u)^2 (1 + 32 u)
//     >= R (1 - u)^26 (1 + 32 u)       (kSlack >= m 2^-1075)
//     >= R (1 - 26 u)(1 + 32 u) = R (1 + 6 u - 832 u^2) >= R.
// A raw bound that is +infinity or NaN stays so.
double finished(double raw) noexcept {
    return (raw + kSlack) * kInflate;
}

Interval exactInterval(double centre) noexcept {
    return Interval{centre, 0.0};
}

std::atomic<bool>& filterSwitch() noexcept {
    // Constant-initialized: no guard at each call.
    static std::atomic<bool> enabled{true};
    return enabled;
}

// The counts of the tests (interval.hpp, startFilterCount()).
struct Counter {
    std::atomic<bool> counting{false};
    std::atomic<std::uint64_t> decided{0};
    std::atomic<std::uint64_t> undecided{0};
};

Counter& counter() noexcept {
    static Counter counts;
    return counts;
}

}  // namespace

Interval unknownInterval() noexcept {
    return Interval{0.0, kInfinity};
}

bool isKnown(const Interval& value) noexcept {
    // False for +infinity and for NaN. A finite radius means a finite centre:
    // every operation that overflows its centre makes the radius infinite.
    return value.radius < kInfinity;
}

Interval intervalOf(double value) noexcept {
    return exactInterval(value);
}

// c = nearest double to X (Dyadic::toDouble rounds as IEEE 754 does; a value
// beyond the largest double gives +-infinity). Fact 1: R = u |c| + 2^-1075.
// Raw bound r = fl(u |c|) >= u |c| - 2^-1075 = R - 2 * 2^-1075 (the product by
// a power of two is exact unless its result is subnormal). k = 0, m = 2.
Interval intervalOf(const Dyadic& value) noexcept {
    const double centre = value.toDouble();
    return Interval{centre, finished(kUnit * std::fabs(centre))};
}

// c = fl(a + b). |A + B - c| <= |A - a| + |B - b| + |a + b - c|, so by Fact 1
// R = ra + rb + u |c|. Raw bound r = fl(fl(ra + rb) + fl(u |c|))
//     >= ((ra + rb)(1 - u) + u |c| - 2^-1075)(1 - u) >= R (1 - u)^2 - 2^-1075.
// k = 2, m = 1.
Interval operator+(const Interval& a, const Interval& b) noexcept {
    const double centre = a.centre + b.centre;
    const double raw = (a.radius + b.radius) + (kUnit * std::fabs(centre));
    return Interval{centre, finished(raw)};
}

// a + (-b): the negation is exact.
Interval operator-(const Interval& a, const Interval& b) noexcept {
    return a + (-b);
}

Interval operator-(const Interval& a) noexcept {
    return Interval{-a.centre, a.radius};
}

// c = fl(a b). With A = a + e, B = b + f, |e| <= ra, |f| <= rb:
//     |A B - a b| = |a f + b e + e f| <= |a| rb + |b| ra + ra rb,
// so by Fact 1 R = |a| rb + |b| ra + ra rb + u |c| + 2^-1075. Raw bound
//     r = fl(fl(fl(fl(|a| rb) + fl(|b| ra)) + fl(ra rb)) + fl(u |c|)),
// three sums and four products of non-negative doubles:
//     r >= R (1 - u)^4 - 5 * 2^-1075.   k = 4, m = 5.
Interval operator*(const Interval& a, const Interval& b) noexcept {
    const double centre = a.centre * b.centre;
    const double raw = (((std::fabs(a.centre) * b.radius) + (std::fabs(b.centre) * a.radius)) +
                        (a.radius * b.radius)) +
                       (kUnit * std::fabs(centre));
    return Interval{centre, finished(raw)};
}

// c = fl(a / b), g = fl(|b| - rb) > 0, so that |B| >= |b| - rb > 0 (rounding
// is monotone and fl(0) = 0, so g > 0 only when |b| - rb > 0). With A = a + e
// and B = b + f:
//     A / B - a / b = (e b - a f) / (B b),
//     |A / B - a / b| <= (ra |b| + |a| rb) / (|B| |b|) <= (ra + |a / b| rb) / (|b| - rb).
// c is normal, or a = 0 and then c = a / b = 0 exactly; otherwise (a nonzero
// quotient below the normal range) the interval is nothing. So Fact 1 holds
// without its absolute term: |a / b - c| <= u |c|, and |a / b| <= |c| (1 + u).
// R = N / (|b| - rb) + u |c|, with N = ra + |c| (1 + u) rb.
//
// The numerator first: n = finished(fl(ra + fl(|c| rb))), whose raw bound is
//     >= (ra + |c| rb (1 - u) - 2^-1075)(1 - u) >= N (1 - u)^3 - 2^-1075,
// since (1 - u)^2 >= (1 + u)(1 - u)^3; so n >= N. Then, by Fact 1 for the
// difference, |b| - rb >= g (1 - u), so n / g >= (n / (|b| - rb))(1 - u), and
// the raw bound r = fl(fl(n / g) + fl(u |c|)) satisfies
//     r >= ((n / (|b| - rb))(1 - u)^2 - 2^-1075 + u |c| - 2^-1075)(1 - u)
//       >= R (1 - u)^3 - 2 * 2^-1075.   k = 3, m = 2.
Interval quotient(const Interval& numerator, const Interval& denominator) noexcept {
    const double gap = std::fabs(denominator.centre) - denominator.radius;
    if (!(gap > 0.0)) {
        return unknownInterval();
    }
    const double centre = numerator.centre / denominator.centre;
    if (numerator.centre != 0.0 && !(std::fabs(centre) >= kSmallestNormal)) {
        return unknownInterval();
    }
    const double top = finished(numerator.radius + (std::fabs(centre) * denominator.radius));
    const double raw = (top / gap) + (kUnit * std::fabs(centre));
    return Interval{centre, finished(raw)};
}

// s = fl(sqrt(a)) for a > 0. a >= 2^-1074, so sqrt(a) >= 2^-537 and s is
// normal: std::sqrt is correctly rounded, |s - sqrt(a)| <= u s, so
// sqrt(a) >= s (1 - u). The exact A is not negative, so
//     |sqrt(A) - sqrt(a)| = |A - a| / (sqrt(A) + sqrt(a)) <= ra / sqrt(a) <= ra / (s (1 - u)),
// and R = ra / (s (1 - u)) + u s. Raw bound r = fl(fl(ra / s) + u s), where
// u s >= 2^-590 is exact:
//     r >= ((ra / s)(1 - u) - 2^-1075 + u s)(1 - u) >= R (1 - u)^3 - 2^-1075.
// k = 3, m = 1.
Interval squareRoot(const Interval& radicand) noexcept {
    if (!(radicand.centre > 0.0)) {
        return unknownInterval();
    }
    const double root = std::sqrt(radicand.centre);
    const double raw = (radicand.radius / root) + (kUnit * root);
    return Interval{root, finished(raw)};
}

// |X - c| <= r < |c| puts X strictly on the side of c. The test fails when r
// is +infinity or NaN.
std::optional<Sign> certainSign(const Interval& value) noexcept {
    if (std::fabs(value.centre) > value.radius) {
        return value.centre > 0.0 ? Sign::positive : Sign::negative;
    }
    return std::nullopt;
}

bool filterEnabled() noexcept {
    return filterSwitch().load(std::memory_order_relaxed);
}

void enableFilter(bool enabled) noexcept {
    filterSwitch().store(enabled, std::memory_order_relaxed);
}

void startFilterCount() noexcept {
    auto& counts = counter();
    counts.decided.store(0, std::memory_order_relaxed);
    counts.undecided.store(0, std::memory_order_relaxed);
    counts.counting.store(true, std::memory_order_relaxed);
}

FilterCounts stopFilterCount() noexcept {
    auto& counts = counter();
    counts.counting.store(false, std::memory_order_relaxed);
    return FilterCounts{counts.decided.load(std::memory_order_relaxed),
                        counts.undecided.load(std::memory_order_relaxed)};
}

void recordFilterAnswer(bool decided) noexcept {
    auto& counts = counter();
    if (counts.counting.load(std::memory_order_relaxed)) {
        (decided ? counts.decided : counts.undecided).fetch_add(1, std::memory_order_relaxed);
    }
}

}  // namespace solid_space::geom2d::exact
