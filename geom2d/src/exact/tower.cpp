// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Numbers of a tower of square roots (tower.hpp; stage 0.2, package P12).
//
// Every function below works on the coefficients of a number written at a
// given level n: 2^n of them, the lower half the part without the top root
// sqrt(r_{n-1}) and the upper half its coefficient. So a number of level n is
// a + b * sqrt(r) with a and b of level n - 1, and every operation recurses on
// the halves.

#include "tower.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace solid_space::geom2d::exact {
namespace {

using robust::Sign;
using Coefficients = std::vector<Dyadic>;

// Bits of the square roots used by the rounding. Each approximation of level n
// has a relative error E_n with E_0 = 0 (exact) and, for a + b * sqrt(r) of
// level n + 1:
// - sqrt(r): |sqrt(1 + e) - 1| <= |e|, so E_n from r, times the floor of
//   floorSqrt, below delta = 2^(1 - kRootBits);
// - b * sqrt(r): E_n + (E_n + delta) plus their product;
// - a + b * sqrt(r) with terms of the same sign: the larger of the two errors;
// - with opposite signs, (a^2 - b^2 r) / (a - b * sqrt(r)): E_n for the
//   numerator, 2 E_n + delta for the denominator, and their quotient.
// So E_{n+1} <= 3 E_n + delta, up to products of errors far below 2^-100, and
// E_n <= (3^n - 1) / 2 * delta: with kMaxTowerRoots = 6, 364 * 2^-127, below
// 2^-118. The final rounding of the quotient adds half an ulp: within one ulp
// in all, as tower.hpp promises (within two).
constexpr std::int64_t kRootBits = 128;

Sign negated(Sign sign) noexcept {
    switch (sign) {
        case Sign::negative:
            return Sign::positive;
        case Sign::positive:
            return Sign::negative;
        case Sign::zero:
            break;
    }
    return Sign::zero;
}

std::size_t levelOfSize(std::size_t size) noexcept {
    std::size_t level = 0;
    while ((std::size_t{1} << level) < size) {
        ++level;
    }
    return level;
}

bool allZero(std::span<const Dyadic> coefficients) noexcept {
    return std::all_of(coefficients.begin(), coefficients.end(),
                       [](const Dyadic& value) { return value.isZero(); });
}

// The coefficients of x written at level n: 2^n of them, zeros above the
// stored ones. Precondition: x.level() <= n.
Coefficients padded(const TowerNumber& x, std::size_t level) {
    const auto stored = x.coefficients();
    assert(stored.size() <= (std::size_t{1} << level) && "a number above the level");
    Coefficients result(std::size_t{1} << level);
    std::copy(stored.begin(), stored.end(), result.begin());
    return result;
}

Coefficients lowerHalf(std::span<const Dyadic> x) {
    return {x.begin(), x.begin() + static_cast<std::ptrdiff_t>(x.size() / 2)};
}

Coefficients upperHalf(std::span<const Dyadic> x) {
    return {x.begin() + static_cast<std::ptrdiff_t>(x.size() / 2), x.end()};
}

Coefficients joined(Coefficients lower, const Coefficients& upper) {
    lower.insert(lower.end(), upper.begin(), upper.end());
    return lower;
}

Coefficients sum(std::span<const Dyadic> a, std::span<const Dyadic> b) {
    assert(a.size() == b.size());
    Coefficients result(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        result[i] = a[i] + b[i];
    }
    return result;
}

Coefficients difference(std::span<const Dyadic> a, std::span<const Dyadic> b) {
    assert(a.size() == b.size());
    Coefficients result(a.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        result[i] = a[i] - b[i];
    }
    return result;
}

// The roots of a tower, with the radicands written at the level of their own
// index, ready for the recursions below.
class Roots {
public:
    explicit Roots(std::span<const TowerNumber> radicands) {
        m_radicands.reserve(radicands.size());
        for (std::size_t i = 0; i < radicands.size(); ++i) {
            m_radicands.push_back(padded(radicands[i], i));
        }
    }

    // r_i written at level i.
    [[nodiscard]] const Coefficients& radicand(std::size_t index) const {
        return m_radicands[index];
    }

    // a * b, both written at level n; the result is written at level n too.
    // (a0 + a1 s)(b0 + b1 s) = (a0 b0 + a1 b1 r) + (a0 b1 + a1 b0) s, with the
    // middle term as (a0 + a1)(b0 + b1) - a0 b0 - a1 b1 (three products, not
    // four).
    // Recursion follows the levels of the tower, at most kMaxTowerRoots deep.
    // NOLINTNEXTLINE(misc-no-recursion)
    [[nodiscard]] Coefficients multiply(std::span<const Dyadic> a, std::span<const Dyadic> b,
                                        std::size_t level) const {
        assert(a.size() == (std::size_t{1} << level) && b.size() == a.size());
        if (level == 0) {
            return {a[0] * b[0]};
        }
        if (allZero(a) || allZero(b)) {
            return Coefficients(a.size());
        }
        const std::size_t lower = level - 1;
        const auto a0 = lowerHalf(a);
        const auto a1 = upperHalf(a);
        const auto b0 = lowerHalf(b);
        const auto b1 = upperHalf(b);
        const bool a1Zero = allZero(a1);
        const bool b1Zero = allZero(b1);
        if (a1Zero && b1Zero) {
            return joined(multiply(a0, b0, lower), Coefficients(a0.size()));
        }
        if (a1Zero) {
            return joined(multiply(a0, b0, lower), multiply(a0, b1, lower));
        }
        if (b1Zero) {
            return joined(multiply(a0, b0, lower), multiply(a1, b0, lower));
        }
        const auto p00 = multiply(a0, b0, lower);
        const auto p11 = multiply(a1, b1, lower);
        const auto middle = multiply(sum(a0, a1), sum(b0, b1), lower);
        const auto radical = difference(difference(middle, p00), p11);
        const auto rational = sum(p00, multiply(p11, m_radicands[lower], lower));
        return joined(rational, radical);
    }

    // a^2 - b^2 r for x = a + b * sqrt(r) written at level n + 1, at level n.
    [[nodiscard]] Coefficients conjugateProduct(std::span<const Dyadic> a,
                                                std::span<const Dyadic> b,
                                                std::size_t level) const {
        const auto bSquared = multiply(b, b, level);
        return difference(multiply(a, a, level), multiply(bSquared, m_radicands[level], level));
    }

    // The sign of x written at level n. With x = a + b sqrt(r) and r > 0 (every
    // root of a tower has a positive radicand), the sign is the one of a when
    // b is zero or of the same sign; the one of b when a is zero; otherwise
    // the sign of a times the sign of a^2 - b^2 r.
    // Recursion follows the levels of the tower, at most kMaxTowerRoots deep.
    // NOLINTNEXTLINE(misc-no-recursion)
    [[nodiscard]] Sign sign(std::span<const Dyadic> x, std::size_t level) const {
        if (level == 0) {
            return x[0].sign();
        }
        const std::size_t lower = level - 1;
        const auto a = lowerHalf(x);
        const auto b = upperHalf(x);
        if (allZero(b)) {
            return sign(a, lower);
        }
        const Sign signA = sign(a, lower);
        const Sign signB = sign(b, lower);
        if (signB == Sign::zero || signA == signB) {
            return signA;
        }
        if (signA == Sign::zero) {
            return signB;
        }
        const Sign squares = sign(conjugateProduct(a, b, lower), lower);
        return signA == Sign::positive ? squares : negated(squares);
    }

private:
    std::vector<Coefficients> m_radicands;
};

// An approximation numerator / denominator of a number, with a positive
// denominator and the relative error of the comment of kRootBits. The
// numerator is zero exactly when the number is.
struct Fraction {
    Dyadic numerator;
    Dyadic denominator = Dyadic(1.0);
};

Fraction times(const Fraction& a, const Fraction& b) {
    return {a.numerator * b.numerator, a.denominator * b.denominator};
}

Fraction plus(const Fraction& a, const Fraction& b) {
    return {a.numerator * b.denominator + b.numerator * a.denominator,
            a.denominator * b.denominator};
}

Fraction minus(const Fraction& a, const Fraction& b) {
    return {a.numerator * b.denominator - b.numerator * a.denominator,
            a.denominator * b.denominator};
}

// a / b. Precondition: b is not zero.
Fraction divided(const Fraction& a, const Fraction& b) {
    assert(!b.numerator.isZero());
    Dyadic numerator = a.numerator * b.denominator;
    Dyadic denominator = a.denominator * b.numerator;
    if (denominator.sign() == Sign::negative) {
        numerator = -numerator;
        denominator = -denominator;
    }
    return {std::move(numerator), std::move(denominator)};
}

// The approximations of one rounding, with the approximations of the roots
// computed once each.
class Approximator {
public:
    explicit Approximator(const Roots& roots, std::size_t size) : m_roots(roots), m_cache(size) {}

    // The approximation of x written at level n, without cancellation: the
    // exact signs choose between a sum of terms of the same sign and the
    // conjugate form (tower.hpp, "Rounding").
    // Recursion follows the levels of the tower, at most kMaxTowerRoots deep.
    // NOLINTNEXTLINE(misc-no-recursion)
    [[nodiscard]] Fraction approximate(std::span<const Dyadic> x, std::size_t level) {
        if (level == 0) {
            return {x[0], Dyadic(1.0)};
        }
        const std::size_t lower = level - 1;
        const auto a = lowerHalf(x);
        const auto b = upperHalf(x);
        if (allZero(b)) {
            return approximate(a, lower);
        }
        const Sign signB = m_roots.sign(b, lower);
        if (signB == Sign::zero) {
            return approximate(a, lower);
        }
        Fraction radical = times(approximate(b, lower), root(lower));
        const Sign signA = m_roots.sign(a, lower);
        if (signA == Sign::zero) {
            return radical;
        }
        if (signA == signB) {
            return plus(approximate(a, lower), radical);
        }
        // a + b sqrt(r) = (a^2 - b^2 r) / (a - b sqrt(r)), the denominator a sum
        // of two terms of the sign of a.
        const Fraction numerator = approximate(m_roots.conjugateProduct(a, b, lower), lower);
        if (numerator.numerator.isZero()) {
            return {Dyadic(), Dyadic(1.0)};
        }
        return divided(numerator, minus(approximate(a, lower), radical));
    }

private:
    // sqrt(r_i) ~ sqrt(p / q) = sqrt(p q) / q.
    // Recursion follows the levels of the tower, at most kMaxTowerRoots deep.
    // NOLINTNEXTLINE(misc-no-recursion)
    const Fraction& root(std::size_t index) {
        auto& cached = m_cache[index];
        if (!cached) {
            const Fraction radicand = approximate(m_roots.radicand(index), index);
            cached = Fraction{floorSqrt(radicand.numerator * radicand.denominator, kRootBits),
                              radicand.denominator};
        }
        return *cached;
    }

    const Roots& m_roots;
    std::vector<std::optional<Fraction>> m_cache;
};

}  // namespace

TowerNumber::TowerNumber(const Dyadic& value) {
    if (!value.isZero()) {
        m_coefficients.push_back(value);
    }
}

TowerNumber::TowerNumber(std::vector<Dyadic> coefficients)
    : m_coefficients(std::move(coefficients)) {
    trim();
}

std::size_t TowerNumber::level() const noexcept {
    return levelOfSize(m_coefficients.size());
}

Dyadic TowerNumber::coefficient(std::size_t mask) const {
    return mask < m_coefficients.size() ? m_coefficients[mask] : Dyadic();
}

TowerNumber TowerNumber::monomial(std::size_t mask, const Dyadic& factor) {
    std::vector<Dyadic> coefficients(std::size_t{1} << levelOfSize(mask + 1));
    coefficients[mask] = factor;
    return TowerNumber(std::move(coefficients));
}

TowerNumber TowerNumber::timesPowerOfTwo(std::int64_t power) const {
    std::vector<Dyadic> coefficients;
    coefficients.reserve(m_coefficients.size());
    for (const auto& value : m_coefficients) {
        coefficients.push_back(value.timesPowerOfTwo(power));
    }
    return TowerNumber(std::move(coefficients));
}

void TowerNumber::trim() {
    while (m_coefficients.size() > 1 &&
           allZero(std::span<const Dyadic>(m_coefficients).subspan(m_coefficients.size() / 2))) {
        m_coefficients.resize(m_coefficients.size() / 2);
    }
    if (m_coefficients.size() == 1 && m_coefficients[0].isZero()) {
        m_coefficients.clear();
    }
}

TowerNumber operator+(const TowerNumber& a, const TowerNumber& b) {
    const std::size_t level = std::max(a.level(), b.level());
    return TowerNumber(sum(padded(a, level), padded(b, level)));
}

TowerNumber operator-(const TowerNumber& a, const TowerNumber& b) {
    const std::size_t level = std::max(a.level(), b.level());
    return TowerNumber(difference(padded(a, level), padded(b, level)));
}

TowerNumber operator-(const TowerNumber& a) {
    std::vector<Dyadic> coefficients;
    coefficients.reserve(a.m_coefficients.size());
    for (const auto& value : a.m_coefficients) {
        coefficients.push_back(-value);
    }
    return TowerNumber(std::move(coefficients));
}

TowerNumber operator*(const TowerNumber& a, const Dyadic& factor) {
    std::vector<Dyadic> coefficients;
    coefficients.reserve(a.m_coefficients.size());
    for (const auto& value : a.m_coefficients) {
        coefficients.push_back(value * factor);
    }
    return TowerNumber(std::move(coefficients));
}

TowerNumber operator*(const Dyadic& factor, const TowerNumber& a) {
    return a * factor;
}

bool sameRepresentation(const TowerNumber& a, const TowerNumber& b) {
    return a.m_coefficients.size() == b.m_coefficients.size() &&
           std::equal(a.m_coefficients.begin(), a.m_coefficients.end(), b.m_coefficients.begin(),
                      [](const Dyadic& x, const Dyadic& y) { return compare(x, y) == Sign::zero; });
}

std::optional<TowerNumber> Tower::squareRoot(const TowerNumber& radicand) {
    assert(radicand.level() <= size() && "the radicand is not a number of this tower");
    const Sign radicandSign = sign(radicand);
    assert(radicandSign != Sign::negative && "a square root needs a radicand that is not negative");
    if (radicandSign == Sign::negative) {
        return std::nullopt;
    }
    if (radicandSign == Sign::zero) {
        return TowerNumber();
    }
    if (radicand.isRational()) {
        // floorSqrt gives the exact root of a perfect square (a product of the
        // integer square root and a power of two); the check makes sure.
        const Dyadic value = radicand.coefficient(0);
        const Dyadic root = floorSqrt(value, 2);
        if (exact::compare(root * root, value) == Sign::zero) {
            return TowerNumber(root);
        }
    }
    for (std::size_t i = 0; i < m_radicands.size(); ++i) {
        if (sameRepresentation(m_radicands[i], radicand)) {
            return TowerNumber::monomial(std::size_t{1} << i, Dyadic(1.0));
        }
    }
    if (m_radicands.size() >= kMaxTowerRoots) {
        return std::nullopt;
    }
    m_radicands.push_back(radicand);
    return TowerNumber::monomial(std::size_t{1} << (m_radicands.size() - 1), Dyadic(1.0));
}

TowerNumber Tower::product(const TowerNumber& a, const TowerNumber& b) const {
    const std::size_t level = std::max(a.level(), b.level());
    assert(level <= size() && "a number of another tower");
    const Roots roots(m_radicands);
    std::vector<Dyadic> result = roots.multiply(padded(a, level), padded(b, level), level);
    return TowerNumber(std::move(result));
}

robust::Sign Tower::sign(const TowerNumber& x) const {
    if (x.isRational()) {
        return x.coefficient(0).sign();
    }
    assert(x.level() <= size() && "a number of another tower");
    const Roots roots(m_radicands);
    return roots.sign(x.coefficients(), x.level());
}

robust::Sign Tower::compare(const TowerNumber& a, const TowerNumber& b) const {
    return sign(a - b);
}

double Tower::toDouble(const TowerNumber& x) const {
    return quotientToDouble(x, Dyadic(1.0));
}

double Tower::quotientToDouble(const TowerNumber& x, const Dyadic& divisor) const {
    assert(divisor.sign() == Sign::positive && "the divisor must be positive");
    if (x.coefficients().empty()) {
        return 0.0;
    }
    assert(x.level() <= size() && "a number of another tower");
    const Roots roots(m_radicands);
    Approximator approximator(roots, m_radicands.size());
    const Fraction value = approximator.approximate(x.coefficients(), x.level());
    if (value.numerator.isZero()) {
        return 0.0;
    }
    return exact::quotientToDouble(value.numerator, value.denominator * divisor);
}

}  // namespace solid_space::geom2d::exact
