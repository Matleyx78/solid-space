// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Unit tests of the numbers of a tower of square roots (stage 0.2, package
// P12). The expected values come from identities of exact arithmetic, from the
// one- and two-root signs of exact/quadratic.hpp (P6), and from constants
// worked out apart with Python's decimal module at 80 digits; never from running
// the code under test.

#include "exact/tower.hpp"

#include "exact/dyadic.hpp"
#include "exact/quadratic.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <vector>

namespace {

using solid_space::geom2d::exact::Dyadic;
using solid_space::geom2d::exact::kMaxTowerRoots;
using solid_space::geom2d::exact::QuadraticNumber;
using solid_space::geom2d::exact::Tower;
using solid_space::geom2d::exact::TowerNumber;
using solid_space::robust::Sign;

namespace exact = solid_space::geom2d::exact;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

// sqrt(2) + sqrt(3) to 40 digits (Python, decimal, 80 digits).
constexpr double kSqrt2PlusSqrt3 = 3.1462643699419723423291350657155704455125;
// The double nearest to it, and the exact value of (sqrt(2) + sqrt(3)) minus
// that double: a number about 2^-52 times smaller than its terms.
constexpr double kSqrt2PlusSqrt3Double = 3.1462643699419726;
constexpr double kSqrt2PlusSqrt3Remainder = -2.183666958414914119286788070505e-16;
// t_0 = sqrt(2), t_i = sqrt(1 + t_{i-1}): t_5, six roots deep.
// Not the golden ratio, though within 1e-3 of it: the check that suggests
// std::numbers::phi only compares the values.
// NOLINTNEXTLINE(modernize-use-std-numbers)
constexpr double kNestedChain = 1.6174427985273905633894215295246445906238;

TowerNumber number(double value) {
    return TowerNumber(Dyadic(value));
}

TowerNumber rootOf(Tower& tower, const TowerNumber& radicand) {
    const auto root = tower.squareRoot(radicand);
    EXPECT_TRUE(root.has_value());
    return root.value_or(TowerNumber());
}

TowerNumber rootOf(Tower& tower, double radicand) {
    return rootOf(tower, number(radicand));
}

// Within `ulps` units in the last place of `expected` (a normal double).
void expectWithinUlps(double actual, double expected, int ulps) {
    const double ulp = std::nextafter(std::abs(expected), kInfinity) - std::abs(expected);
    EXPECT_LE(std::abs(actual - expected), ulps * ulp) << actual << " vs " << expected;
}

// SplitMix64: the same deterministic stream on every platform.
class Generator {
public:
    explicit Generator(std::uint64_t seed) : m_state(seed) {}

    std::uint64_t next() {
        m_state += 0x9E3779B97F4A7C15ULL;
        std::uint64_t z = m_state;
        z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31U);
    }

    // A small integer in [-limit, limit].
    std::int64_t integer(std::int64_t limit) {
        return static_cast<std::int64_t>(next() % static_cast<std::uint64_t>(2 * limit + 1)) -
               limit;
    }

private:
    std::uint64_t m_state;
};

TEST(TowerNumber, RepresentationIsTrimmed) {
    EXPECT_TRUE(TowerNumber().coefficients().empty());
    EXPECT_TRUE(number(0.0).coefficients().empty());
    EXPECT_EQ(number(3.0).level(), 0U);
    const auto monomial = TowerNumber::monomial(4, Dyadic(2.0));
    EXPECT_EQ(monomial.level(), 3U);
    EXPECT_EQ(monomial.coefficients().size(), 8U);
    const auto cancelled = monomial - TowerNumber::monomial(4, Dyadic(2.0)) + number(1.5);
    EXPECT_TRUE(cancelled.isRational());
    EXPECT_EQ(compare(cancelled.coefficient(0), Dyadic(1.5)), Sign::zero);
}

TEST(Tower, SquareRootsOfRationals) {
    Tower tower;
    // Perfect squares, zero and repeated radicands add no root.
    EXPECT_TRUE(sameRepresentation(rootOf(tower, 0.0), TowerNumber()));
    EXPECT_TRUE(sameRepresentation(rootOf(tower, 9.0), number(3.0)));
    EXPECT_TRUE(sameRepresentation(rootOf(tower, 0.25), number(0.5)));
    EXPECT_EQ(tower.size(), 0U);
    const auto s2 = rootOf(tower, 2.0);
    EXPECT_EQ(tower.size(), 1U);
    EXPECT_TRUE(sameRepresentation(rootOf(tower, 2.0), s2));
    EXPECT_EQ(tower.size(), 1U);
    EXPECT_EQ(tower.compare(tower.product(s2, s2), number(2.0)), Sign::zero);
    EXPECT_EQ(tower.sign(s2), Sign::positive);
    EXPECT_EQ(tower.sign(-s2), Sign::negative);
}

TEST(Tower, SquareOfASubnormalIsAPerfectSquare) {
    Tower tower;
    const Dyadic tiny(0x1p-1074);
    const auto root = tower.squareRoot(TowerNumber(tiny * tiny));
    ASSERT_TRUE(root.has_value());
    EXPECT_TRUE(sameRepresentation(root.value_or(TowerNumber()), TowerNumber(tiny)));
    EXPECT_EQ(tower.size(), 0U);
}

// sqrt(5 + 2 sqrt(6)) = sqrt(2) + sqrt(3), and (sqrt(2) + sqrt(3))^2 = 5 +
// 2 sqrt(6), with sqrt(6) built both as sqrt(2) sqrt(3) and as a root of its
// own; the same scaled by 2^e towards the subnormals and towards 2^50.
TEST(Tower, NestedIdentitiesHoldExactlyAtEveryScale) {
    for (const std::int64_t e : {-1074, -600, -537, -1, 0, 1, 25, 50, 300}) {
        SCOPED_TRACE(e);
        Tower tower;
        const Dyadic scale = Dyadic(1.0).timesPowerOfTwo(e);
        const Dyadic square = scale * scale;
        const auto s2 = rootOf(tower, TowerNumber(Dyadic(2.0) * square));
        const auto s3 = rootOf(tower, TowerNumber(Dyadic(3.0) * square));
        const auto inner =
            TowerNumber(Dyadic(5.0) * square) + tower.product(s2, s3).timesPowerOfTwo(1);
        const auto nested = rootOf(tower, inner);
        EXPECT_EQ(tower.size(), 3U);
        EXPECT_EQ(tower.sign(nested - s2 - s3), Sign::zero);
        EXPECT_EQ(tower.compare(nested, s2 + s3), Sign::zero);

        const auto sum = s2 + s3;
        EXPECT_EQ(tower.sign(tower.product(sum, sum) - inner), Sign::zero);
        const auto s6 = rootOf(tower, TowerNumber(Dyadic(6.0) * square * square));
        EXPECT_EQ(tower.sign(tower.product(sum, sum) - TowerNumber(Dyadic(5.0) * square) -
                             s6.timesPowerOfTwo(1)),
                  Sign::zero);
        // A hair away from the identity: one unit of the last coefficient.
        const auto off = nested - s2 - s3 + TowerNumber(scale * Dyadic(0x1p-200));
        EXPECT_EQ(tower.sign(off), Sign::positive);
        EXPECT_EQ(tower.sign(-off), Sign::negative);
    }
}

// sqrt(2) + sqrt(3) against the doubles on each side of it, and a nested
// root against a double one ulp away: the signs of P6 (two roots) decide them
// independently.
TEST(Tower, NumbersOneUlpFromZero) {
    Tower tower;
    const auto s2 = rootOf(tower, 2.0);
    const auto s3 = rootOf(tower, 3.0);
    const auto nested = rootOf(tower, number(5.0) + tower.product(s2, s3).timesPowerOfTwo(1));
    for (const double candidate :
         {kSqrt2PlusSqrt3Double, std::nextafter(kSqrt2PlusSqrt3Double, -kInfinity),
          std::nextafter(kSqrt2PlusSqrt3Double, kInfinity)}) {
        SCOPED_TRACE(candidate);
        const Sign expected =
            exact::signOf(QuadraticNumber{Dyadic(-candidate), Dyadic(1.0), Dyadic(2.0)},
                          QuadraticNumber{Dyadic(1.0), Dyadic(), Dyadic()}, Dyadic(3.0));
        EXPECT_EQ(tower.sign(s2 + s3 - number(candidate)), expected);
        EXPECT_EQ(tower.sign(nested - number(candidate)), expected);
    }
    // The double nearest lies above the exact value, the one below it under.
    EXPECT_EQ(tower.sign(nested - number(kSqrt2PlusSqrt3Double)), Sign::negative);
    EXPECT_EQ(tower.sign(nested - number(std::nextafter(kSqrt2PlusSqrt3Double, -kInfinity))),
              Sign::positive);
    // sqrt(2) against the doubles around it.
    const double root2 = std::numbers::sqrt2;
    const Sign below = exact::signOf(Dyadic(-root2), Dyadic(1.0), Dyadic(2.0));
    EXPECT_EQ(tower.sign(s2 - number(root2)), below);
    const double other = std::nextafter(root2, below == Sign::positive ? kInfinity : -kInfinity);
    EXPECT_EQ(tower.sign(s2 - number(other)),
              below == Sign::positive ? Sign::negative : Sign::positive);
}

TEST(Tower, RoundingOfKnownValues) {
    Tower tower;
    const auto s2 = rootOf(tower, 2.0);
    const auto s3 = rootOf(tower, 3.0);
    const auto nested = rootOf(tower, number(5.0) + tower.product(s2, s3).timesPowerOfTwo(1));
    expectWithinUlps(tower.toDouble(s2), std::numbers::sqrt2, 2);
    expectWithinUlps(tower.toDouble(s2 + s3), kSqrt2PlusSqrt3, 2);
    expectWithinUlps(tower.toDouble(nested), kSqrt2PlusSqrt3, 2);
    // The terms cancel to 2^-52 of their size: still within 2 ulp of the
    // result, through the conjugate form.
    expectWithinUlps(tower.toDouble(nested - number(kSqrt2PlusSqrt3Double)),
                     kSqrt2PlusSqrt3Remainder, 2);
    expectWithinUlps(tower.toDouble(s2 + s3 - number(kSqrt2PlusSqrt3Double)),
                     kSqrt2PlusSqrt3Remainder, 2);
    // An exact zero is +0, however it is written.
    const double zero = tower.toDouble(nested - s2 - s3);
    EXPECT_EQ(zero, 0.0);
    EXPECT_FALSE(std::signbit(zero));
    // Quotients, and values beyond the doubles.
    expectWithinUlps(tower.quotientToDouble(nested, Dyadic(3.0)), kSqrt2PlusSqrt3 / 3.0, 2);
    EXPECT_EQ(tower.toDouble(nested * Dyadic(0x1p1023) * Dyadic(4.0)), kInfinity);
    EXPECT_EQ(tower.toDouble(-(nested * Dyadic(0x1p1023) * Dyadic(4.0))), -kInfinity);
    expectWithinUlps(tower.toDouble(nested * Dyadic(0x1p-1000)), kSqrt2PlusSqrt3 * 0x1p-1000, 2);
}

// Products agree with the ring laws exactly, on random numbers of a tower of
// six roots, and the signs agree with the rounding when the value is far from
// zero.
TEST(Tower, RingLawsAndSignsOfRandomNumbers) {
    Tower tower;
    std::vector<TowerNumber> roots;
    for (const double radicand : {2.0, 3.0, 5.0}) {
        roots.push_back(rootOf(tower, radicand));
    }
    roots.push_back(rootOf(tower, number(1.0) + roots[0]));
    roots.push_back(rootOf(tower, number(7.0) - roots[1] - roots[3]));
    roots.push_back(rootOf(tower, tower.product(roots[2], roots[4]) + number(0.5)));
    ASSERT_EQ(tower.size(), kMaxTowerRoots);
    // The values of the roots, in plain doubles, for the far-from-zero check.
    std::vector<double> values{std::numbers::sqrt2, std::numbers::sqrt3, std::sqrt(5.0)};
    values.push_back(std::sqrt(1.0 + values[0]));
    values.push_back(std::sqrt(7.0 - values[1] - values[3]));
    values.push_back(std::sqrt(values[2] * values[4] + 0.5));

    Generator generator(57);
    const auto randomNumber = [&](double& value) {
        TowerNumber x;
        value = 0.0;
        for (std::size_t mask = 0; mask < (std::size_t{1} << kMaxTowerRoots); ++mask) {
            if (generator.next() % 4 != 0) {
                continue;
            }
            const auto coefficient = static_cast<double>(generator.integer(9));
            x = x + TowerNumber::monomial(mask, Dyadic(coefficient));
            double term = coefficient;
            for (std::size_t i = 0; i < kMaxTowerRoots; ++i) {
                if ((mask >> i) % 2 != 0) {
                    term *= values[i];
                }
            }
            value += term;
        }
        return x;
    };
    for (int round = 0; round < 40; ++round) {
        SCOPED_TRACE(round);
        double va = 0.0;
        double vb = 0.0;
        double vc = 0.0;
        const auto a = randomNumber(va);
        const auto b = randomNumber(vb);
        const auto c = randomNumber(vc);
        EXPECT_EQ(tower.sign(tower.product(a, b + c) - tower.product(a, b) - tower.product(a, c)),
                  Sign::zero);
        EXPECT_EQ(tower.sign(tower.product(tower.product(a, b), c) -
                             tower.product(a, tower.product(b, c))),
                  Sign::zero);
        EXPECT_EQ(tower.sign(tower.product(a, b) - tower.product(b, a)), Sign::zero);
        // Far from zero, the doubles agree on the sign and the rounding is
        // close to them (they carry an error of a few hundred ulp at most).
        if (std::abs(va) > 1e-6) {
            const Sign expected = va > 0.0 ? Sign::positive : Sign::negative;
            EXPECT_EQ(tower.sign(a), expected);
            const double rounded = tower.toDouble(a);
            EXPECT_EQ(rounded > 0.0, va > 0.0);
            EXPECT_LE(std::abs(rounded - va), 1e-11 * (1.0 + std::abs(va)));
        }
        const auto square = tower.product(a, a);
        EXPECT_NE(tower.sign(square), Sign::negative);
    }
}

// Six roots fit; a seventh does not, and squareRoot says so. Roots already
// there, perfect squares and zero still work in a full tower, and the signs
// and the rounding work at full depth.
TEST(Tower, DepthLimitIsKept) {
    Tower tower;
    // t_0 = sqrt(2), t_i = sqrt(1 + t_{i-1}).
    TowerNumber chain = rootOf(tower, 2.0);
    for (std::size_t i = 1; i < kMaxTowerRoots; ++i) {
        chain = rootOf(tower, number(1.0) + chain);
    }
    ASSERT_EQ(tower.size(), kMaxTowerRoots);
    EXPECT_FALSE(tower.squareRoot(number(17.0)).has_value());
    EXPECT_FALSE(tower.squareRoot(number(1.0) + chain).has_value());
    EXPECT_EQ(tower.size(), kMaxTowerRoots);
    EXPECT_TRUE(tower.squareRoot(number(16.0)).has_value());
    EXPECT_TRUE(tower.squareRoot(TowerNumber()).has_value());
    EXPECT_TRUE(tower.squareRoot(number(2.0)).has_value());
    // t_5^2 = 1 + t_4, decided at full depth.
    const auto previous = tower.radicands().back() - number(1.0);
    EXPECT_EQ(tower.sign(tower.product(chain, chain) - number(1.0) - previous), Sign::zero);
    expectWithinUlps(tower.toDouble(chain), kNestedChain, 2);
    const double chainDouble = kNestedChain;
    EXPECT_EQ(tower.sign(chain - number(std::nextafter(chainDouble, kInfinity))), Sign::negative);
    EXPECT_EQ(tower.sign(chain - number(std::nextafter(chainDouble, -kInfinity))), Sign::positive);
}

}  // namespace
