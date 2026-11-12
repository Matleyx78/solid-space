// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Unit tests of the real roots of polynomials of degree at most four (stage
// 0.3, package PS3). Every expected value comes from a polynomial built as a
// product of known factors (rational roots, square roots of known numbers,
// powers of two) or from identities of exact arithmetic; never from running
// the code under test.

#include "exact/polynomial.hpp"

#include "exact/dyadic.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>

namespace {

using solid_space::geom2d::exact::compare;
using solid_space::geom2d::exact::Dyadic;
using solid_space::geom2d::exact::greatestCommonDivisor;
using solid_space::geom2d::exact::kMaxDegree;
using solid_space::geom2d::exact::narrowed;
using solid_space::geom2d::exact::Polynomial;
using solid_space::geom2d::exact::RealRoot;
using solid_space::geom2d::exact::realRoots;
using solid_space::geom2d::exact::signAt;
using solid_space::geom2d::exact::squarefreePart;
using solid_space::geom2d::exact::SturmSequence;
using solid_space::geom2d::exact::toDouble;
using solid_space::robust::Sign;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

Dyadic d(double value) {
    return Dyadic(value);
}

// c_0 + c_1 x + ... from doubles, lowest degree first.
Polynomial poly(std::initializer_list<double> coefficients) {
    std::vector<Dyadic> values;
    for (const double c : coefficients) {
        values.emplace_back(c);
    }
    return Polynomial(std::move(values));
}

// x - r.
Polynomial linear(const Dyadic& root) {
    return Polynomial({-root, Dyadic(1.0)});
}

Polynomial linear(double root) {
    return linear(Dyadic(root));
}

// x^2 - c.
Polynomial squareMinus(const Dyadic& c) {
    return Polynomial({-c, Dyadic(), Dyadic(1.0)});
}

Polynomial squareMinus(double c) {
    return squareMinus(Dyadic(c));
}

Dyadic power(std::int64_t exponent) {
    return Dyadic(1.0).timesPowerOfTwo(exponent);
}

bool same(const Dyadic& a, const Dyadic& b) {
    return compare(a, b) == Sign::zero;
}

// The isolating interval contains no root of a polynomial other than the root
// itself and is ordered.
void expectWellFormed(const RealRoot& root) {
    EXPECT_NE(compare(root.lower(), root.upper()), Sign::positive);
    if (root.isExact()) {
        EXPECT_EQ(root.squarefree().signAt(root.lower()), Sign::zero);
    } else {
        // The ends are not roots, and the squarefree part changes sign once.
        EXPECT_NE(root.squarefree().signAt(root.lower()), Sign::zero);
        EXPECT_NE(root.squarefree().signAt(root.upper()), Sign::zero);
        EXPECT_NE(root.squarefree().signAt(root.lower()), root.squarefree().signAt(root.upper()));
        EXPECT_EQ(SturmSequence(root.squarefree()).countRoots(root.lower(), root.upper()), 1U);
    }
}

// The promise of toDouble: within 2^-52 |r| + 2^-1074 of the root. Checked
// against a double `nearest` within half an ulp of the root (a correctly
// rounded sqrt, for instance): the two doubles are then at most one ulp of
// `nearest` apart.
void expectRoundedNear(const RealRoot& root, double nearest) {
    const double value = toDouble(root);
    const double ulp = std::nextafter(std::abs(nearest), kInfinity) - std::abs(nearest);
    EXPECT_LE(std::abs(value - nearest), ulp) << value << " vs " << nearest;
}

// ---------------------------------------------------------------------------
// Arithmetic

TEST(PolynomialTest, DegreeAndLeadingCoefficientAreExact) {
    EXPECT_EQ(Polynomial().degree(), -1);
    EXPECT_TRUE(Polynomial().isZero());
    EXPECT_TRUE(Polynomial().leadingCoefficient().isZero());
    EXPECT_EQ(poly({0.0, 0.0}).degree(), -1);
    EXPECT_EQ(poly({3.0}).degree(), 0);

    // A zero leading coefficient lowers the degree.
    const Polynomial lowered = poly({1.0, 2.0, 0.0, 0.0});
    EXPECT_EQ(lowered.degree(), 1);
    EXPECT_TRUE(same(lowered.leadingCoefficient(), d(2.0)));
    EXPECT_EQ(lowered.coefficients().size(), 2U);
    EXPECT_TRUE(lowered.coefficient(7).isZero());

    // A sum whose leading terms cancel.
    const Polynomial cancelled = poly({1.0, 0.0, 1.0}) - poly({0.0, 5.0, 1.0});
    EXPECT_EQ(cancelled.degree(), 1);
    EXPECT_TRUE(same(cancelled.leadingCoefficient(), d(-5.0)));

    // A leading coefficient that cancels only exactly: 2^-1074 - 2^-1074 x^3.
    const Polynomial tiny = poly({0.0, 0.0, 0.0, 0x1p-1074}) + poly({7.0, 0.0, 0.0, -0x1p-1074});
    EXPECT_EQ(tiny.degree(), 0);
}

TEST(PolynomialTest, SumProductDerivativeAndEvaluation) {
    // (x - 1)(x + 2) = x^2 + x - 2.
    const Polynomial product = linear(1.0) * linear(-2.0);
    EXPECT_EQ(product, poly({-2.0, 1.0, 1.0}));
    EXPECT_EQ(product * Polynomial(), Polynomial());
    EXPECT_EQ(product * d(0.5), poly({-1.0, 0.5, 0.5}));
    EXPECT_EQ(d(2.0) * product, poly({-4.0, 2.0, 2.0}));
    EXPECT_EQ(-product, poly({2.0, -1.0, -1.0}));
    EXPECT_EQ(product + poly({2.0}), poly({0.0, 1.0, 1.0}));

    // (x^4 - 3x^2 + 0.5 x - 7)' = 4x^3 - 6x + 0.5.
    EXPECT_EQ(poly({-7.0, 0.5, -3.0, 0.0, 1.0}).derivative(), poly({0.5, -6.0, 0.0, 4.0}));
    EXPECT_TRUE(poly({5.0}).derivative().isZero());

    // Exact values: x^2 + x - 2 at 1 is 0, at 0.25 is -1.6875, at 2^-1074 is
    // -2 + 2^-1074 + 2^-2148, which a double cannot hold.
    EXPECT_EQ(product.signAt(d(1.0)), Sign::zero);
    EXPECT_TRUE(same(product.evaluate(d(0.25)), d(-1.6875)));
    const Dyadic smallest = power(-1074);
    EXPECT_TRUE(same(product.evaluate(smallest), d(-2.0) + smallest + power(-2148)));
    EXPECT_EQ(product.signAt(d(-2.0)), Sign::zero);
    EXPECT_EQ(product.signAt(d(-3.0)), Sign::positive);
}

// ---------------------------------------------------------------------------
// Divisors and counts

TEST(PolynomialTest, GreatestCommonDivisorAndSquarefreePart) {
    // gcd((x - 1)^2 (x - 2), (x - 1)(x - 3)) = x - 1, up to a factor.
    const Polynomial a = linear(1.0) * linear(1.0) * linear(2.0);
    const Polynomial b = linear(1.0) * linear(3.0);
    const Polynomial divisor = greatestCommonDivisor(a, b);
    EXPECT_EQ(divisor.degree(), 1);
    EXPECT_EQ(divisor.signAt(d(1.0)), Sign::zero);

    // Coprime: a nonzero constant. With zero: the other one.
    EXPECT_EQ(greatestCommonDivisor(linear(1.0), linear(2.0)).degree(), 0);
    EXPECT_EQ(greatestCommonDivisor(a, Polynomial()), a);
    EXPECT_TRUE(greatestCommonDivisor(Polynomial(), Polynomial()).isZero());

    // (x - 1)^3 (x + 2): squarefree part of degree two with roots 1 and -2.
    const Polynomial cube = linear(1.0) * linear(1.0) * linear(1.0) * linear(-2.0);
    const Polynomial squarefree = squarefreePart(cube);
    EXPECT_EQ(squarefree.degree(), 2);
    EXPECT_EQ(squarefree.signAt(d(1.0)), Sign::zero);
    EXPECT_EQ(squarefree.signAt(d(-2.0)), Sign::zero);

    // Already squarefree: unchanged.
    EXPECT_EQ(squarefreePart(b), b);
    EXPECT_EQ(squarefreePart(poly({4.0})), poly({4.0}));
}

TEST(PolynomialTest, SturmCountsDistinctRealRoots) {
    // (x - 1)(x - 2)(x - 3)(x - 4).
    const SturmSequence four(linear(1.0) * linear(2.0) * linear(3.0) * linear(4.0));
    EXPECT_EQ(four.countRoots(), 4U);
    EXPECT_EQ(four.countRoots(d(1.0), d(4.0)), 4U);
    EXPECT_EQ(four.countRoots(d(1.5), d(3.5)), 2U);
    EXPECT_EQ(four.countRoots(d(1.0), d(1.0)), 1U);
    EXPECT_EQ(four.countRoots(d(1.25), d(1.75)), 0U);
    EXPECT_EQ(four.countRoots(d(4.0), d(100.0)), 1U);
    EXPECT_EQ(four.countRoots(d(-100.0), d(1.0)), 1U);

    // Multiple roots count once: (x - 1)^2 (x - 2), and with a negative
    // leading coefficient.
    const Polynomial repeated = linear(1.0) * linear(1.0) * linear(2.0);
    EXPECT_EQ(SturmSequence(repeated).countRoots(), 2U);
    EXPECT_EQ(SturmSequence(-repeated).countRoots(), 2U);
    EXPECT_EQ(SturmSequence(-repeated).countRoots(d(0.0), d(1.0)), 1U);

    // Complex roots only: x^2 + 1, x^4 + 1, (x^2 + 1)^2.
    EXPECT_EQ(SturmSequence(poly({1.0, 0.0, 1.0})).countRoots(), 0U);
    EXPECT_EQ(SturmSequence(poly({1.0, 0.0, 0.0, 0.0, 1.0})).countRoots(), 0U);
    EXPECT_EQ(SturmSequence(poly({1.0, 0.0, 2.0, 0.0, 1.0})).countRoots(), 0U);
    EXPECT_EQ(SturmSequence(poly({5.0})).countRoots(), 0U);

    // (x^2 + 1)(x^2 - 2): two real roots, +-sqrt(2), in [-1.5, 1.5].
    const SturmSequence mixed(poly({1.0, 0.0, 1.0}) * squareMinus(2.0));
    EXPECT_EQ(mixed.countRoots(), 2U);
    EXPECT_EQ(mixed.countRoots(d(-1.5), d(1.5)), 2U);
    EXPECT_EQ(mixed.countRoots(d(-1.4), d(1.4)), 0U);
}

// ---------------------------------------------------------------------------
// Isolation

TEST(PolynomialTest, FourDistinctRealRoots) {
    // (x^2 - 2)(x^2 - 3): -sqrt(3), -sqrt(2), sqrt(2), sqrt(3).
    const std::vector<RealRoot> roots = realRoots(squareMinus(2.0) * squareMinus(3.0));
    ASSERT_EQ(roots.size(), 4U);
    for (const RealRoot& root : roots) {
        expectWellFormed(root);
        EXPECT_EQ(root.multiplicity(), 1);
    }
    for (std::size_t i = 0; i + 1 < roots.size(); ++i) {
        EXPECT_EQ(compare(roots[i], roots[i + 1]), Sign::negative);
        // Isolating intervals do not overlap.
        EXPECT_NE(compare(roots[i].upper(), roots[i + 1].lower()), Sign::positive);
    }
    EXPECT_EQ(signAt(squareMinus(3.0), roots[0]), Sign::zero);
    EXPECT_EQ(signAt(squareMinus(2.0), roots[1]), Sign::zero);
    EXPECT_EQ(signAt(squareMinus(2.0), roots[2]), Sign::zero);
    EXPECT_EQ(signAt(squareMinus(3.0), roots[3]), Sign::zero);
    expectRoundedNear(roots[0], -std::numbers::sqrt3);
    expectRoundedNear(roots[1], -std::numbers::sqrt2);
    expectRoundedNear(roots[2], std::numbers::sqrt2);
    expectRoundedNear(roots[3], std::numbers::sqrt3);

    // Integer roots: (x - 1)(x - 2)(x - 3)(x - 4) with a negative factor.
    const std::vector<RealRoot> integers =
        realRoots(d(-3.0) * linear(4.0) * linear(2.0) * linear(1.0) * linear(3.0));
    ASSERT_EQ(integers.size(), 4U);
    for (std::size_t i = 0; i < integers.size(); ++i) {
        expectWellFormed(integers[i]);
        const auto expected = static_cast<double>(i + 1);
        EXPECT_EQ(compare(integers[i], d(expected)), Sign::zero);
        EXPECT_EQ(toDouble(integers[i]), expected);
    }
}

TEST(PolynomialTest, TwoDoubleRoots) {
    // (x - 1)^2 (x + 3)^2.
    const Polynomial dyadicRoots = linear(1.0) * linear(1.0) * linear(-3.0) * linear(-3.0);
    const std::vector<RealRoot> roots = realRoots(dyadicRoots);
    ASSERT_EQ(roots.size(), 2U);
    EXPECT_EQ(compare(roots[0], d(-3.0)), Sign::zero);
    EXPECT_EQ(compare(roots[1], d(1.0)), Sign::zero);
    EXPECT_EQ(roots[0].multiplicity(), 2);
    EXPECT_EQ(roots[1].multiplicity(), 2);

    // (x^2 - 2)^2: +-sqrt(2), double, not dyadic.
    const std::vector<RealRoot> irrational = realRoots(squareMinus(2.0) * squareMinus(2.0));
    ASSERT_EQ(irrational.size(), 2U);
    for (const RealRoot& root : irrational) {
        expectWellFormed(root);
        EXPECT_FALSE(root.isExact());
        EXPECT_EQ(root.multiplicity(), 2);
        EXPECT_EQ(signAt(squareMinus(2.0), root), Sign::zero);
    }
    expectRoundedNear(irrational[0], -std::numbers::sqrt2);
    expectRoundedNear(irrational[1], std::numbers::sqrt2);
}

TEST(PolynomialTest, TripleAndQuadrupleRoots) {
    // (x - 0.5)^3 (x + 1).
    const Polynomial triple = linear(0.5) * linear(0.5) * linear(0.5) * linear(-1.0);
    const std::vector<RealRoot> roots = realRoots(triple);
    ASSERT_EQ(roots.size(), 2U);
    EXPECT_EQ(compare(roots[0], d(-1.0)), Sign::zero);
    EXPECT_EQ(roots[0].multiplicity(), 1);
    EXPECT_EQ(compare(roots[1], d(0.5)), Sign::zero);
    EXPECT_EQ(roots[1].multiplicity(), 3);

    // (3x - 1)^4: 1/3, not dyadic, of multiplicity four.
    const Polynomial factor = poly({-1.0, 3.0});
    const std::vector<RealRoot> third = realRoots(factor * factor * factor * factor);
    ASSERT_EQ(third.size(), 1U);
    expectWellFormed(third[0]);
    EXPECT_EQ(third[0].multiplicity(), 4);
    EXPECT_EQ(signAt(factor, third[0]), Sign::zero);
    expectRoundedNear(third[0], 1.0 / 3.0);

    // (x - 0.75)^4, dyadic.
    const Polynomial quarter = linear(0.75);
    const std::vector<RealRoot> quadruple = realRoots(quarter * quarter * quarter * quarter);
    ASSERT_EQ(quadruple.size(), 1U);
    EXPECT_EQ(quadruple[0].multiplicity(), 4);
    EXPECT_EQ(toDouble(quadruple[0]), 0.75);
}

TEST(PolynomialTest, ComplexRootsAreNotReal) {
    EXPECT_TRUE(realRoots(poly({1.0, 0.0, 1.0})).empty());
    EXPECT_TRUE(realRoots(poly({1.0, 0.0, 0.0, 0.0, 1.0})).empty());
    EXPECT_TRUE(realRoots(poly({1.0, 0.0, 2.0, 0.0, 1.0})).empty());
    EXPECT_TRUE(realRoots(poly({-2.0})).empty());

    // (x^2 + 1)(x - 2)^2: one real root, double; the complex pair ignored.
    const std::vector<RealRoot> roots =
        realRoots(poly({1.0, 0.0, 1.0}) * linear(2.0) * linear(2.0));
    ASSERT_EQ(roots.size(), 1U);
    EXPECT_EQ(compare(roots[0], d(2.0)), Sign::zero);
    EXPECT_EQ(roots[0].multiplicity(), 2);
}

TEST(PolynomialTest, RootAtZeroAndLinear) {
    // x (x - 1): zero is the first midpoint of the search.
    const std::vector<RealRoot> roots = realRoots(poly({0.0, -1.0, 1.0}));
    ASSERT_EQ(roots.size(), 2U);
    EXPECT_TRUE(roots[0].isExact());
    EXPECT_EQ(compare(roots[0], d(0.0)), Sign::zero);
    EXPECT_EQ(toDouble(roots[0]), 0.0);
    EXPECT_EQ(signAt(linear(-1.0), roots[0]), Sign::positive);
    EXPECT_EQ(compare(roots[1], d(1.0)), Sign::zero);

    // 3x + 1: -1/3.
    const std::vector<RealRoot> linearRoot = realRoots(poly({1.0, 3.0}));
    ASSERT_EQ(linearRoot.size(), 1U);
    expectRoundedNear(linearRoot[0], -1.0 / 3.0);

    // 5 x^4: zero, quadruple.
    const std::vector<RealRoot> monomial = realRoots(poly({0.0, 0.0, 0.0, 0.0, 5.0}));
    ASSERT_EQ(monomial.size(), 1U);
    EXPECT_EQ(compare(monomial[0], d(0.0)), Sign::zero);
    EXPECT_EQ(monomial[0].multiplicity(), 4);
}

TEST(PolynomialTest, RootsVeryCloseTogetherAreSeparated) {
    // (x^2 - 2)((x - 2^-60)^2 - 2): -sqrt(2), -sqrt(2) + 2^-60, sqrt(2),
    // sqrt(2) + 2^-60.
    const Dyadic shift = power(-60);
    const Polynomial shifted = Polynomial({shift * shift - d(2.0), d(-2.0) * shift, d(1.0)});
    const std::vector<RealRoot> roots = realRoots(squareMinus(2.0) * shifted);
    ASSERT_EQ(roots.size(), 4U);
    for (const RealRoot& root : roots) {
        expectWellFormed(root);
        EXPECT_EQ(root.multiplicity(), 1);
    }
    EXPECT_EQ(signAt(squareMinus(2.0), roots[0]), Sign::zero);
    EXPECT_EQ(signAt(shifted, roots[1]), Sign::zero);
    EXPECT_EQ(signAt(squareMinus(2.0), roots[2]), Sign::zero);
    EXPECT_EQ(signAt(shifted, roots[3]), Sign::zero);
    EXPECT_EQ(compare(roots[2], roots[3]), Sign::negative);
    EXPECT_EQ(compare(roots[3], roots[2]), Sign::positive);

    // Two dyadic roots 2^-60 apart: 1 and 1 + 2^-60.
    const Dyadic nextToOne = d(1.0) + shift;
    const std::vector<RealRoot> dyadicPair = realRoots(linear(1.0) * linear(nextToOne));
    ASSERT_EQ(dyadicPair.size(), 2U);
    EXPECT_EQ(compare(dyadicPair[0], d(1.0)), Sign::zero);
    EXPECT_EQ(compare(dyadicPair[1], nextToOne), Sign::zero);

    // A double root next to a simple one: (x - 1)^2 (x - 1 - 2^-60).
    const std::vector<RealRoot> nearDouble =
        realRoots(linear(1.0) * linear(1.0) * linear(nextToOne));
    ASSERT_EQ(nearDouble.size(), 2U);
    EXPECT_EQ(nearDouble[0].multiplicity(), 2);
    EXPECT_EQ(nearDouble[1].multiplicity(), 1);
    EXPECT_EQ(compare(nearDouble[1], nextToOne), Sign::zero);
}

TEST(PolynomialTest, CoefficientsWithVeryDifferentExponents) {
    // (x^2 - 3 * 2^-2148)(x - 2^200): roots -+sqrt(3) * 2^-1074, below the
    // smallest positive double but one, and 2^200.
    const Polynomial tiny = squareMinus(d(3.0) * power(-2148));
    const std::vector<RealRoot> roots = realRoots(tiny * linear(power(200)));
    ASSERT_EQ(roots.size(), 3U);
    for (const RealRoot& root : roots) {
        expectWellFormed(root);
    }
    EXPECT_EQ(signAt(tiny, roots[0]), Sign::zero);
    EXPECT_EQ(signAt(tiny, roots[1]), Sign::zero);
    EXPECT_EQ(compare(roots[1], power(-1074)), Sign::positive);
    EXPECT_EQ(compare(roots[1], power(-1073)), Sign::negative);
    EXPECT_EQ(compare(roots[2], power(200)), Sign::zero);
    // sqrt(3) * 2^-1074 rounds to two subnormal units.
    EXPECT_EQ(toDouble(roots[1]), 0x1p-1073);
    EXPECT_EQ(toDouble(roots[0]), -0x1p-1073);
    EXPECT_EQ(toDouble(roots[2]), 0x1p200);

    // Roots beyond the largest double: x^2 - 3 * 2^2100, +-sqrt(3) * 2^1050.
    const std::vector<RealRoot> huge = realRoots(squareMinus(d(3.0) * power(2100)));
    ASSERT_EQ(huge.size(), 2U);
    EXPECT_EQ(toDouble(huge[0]), -kInfinity);
    EXPECT_EQ(toDouble(huge[1]), kInfinity);

    // Roots below half the smallest subnormal: x^2 - 2^-2200.
    const std::vector<RealRoot> vanishing = realRoots(squareMinus(power(-2200)));
    ASSERT_EQ(vanishing.size(), 2U);
    EXPECT_EQ(toDouble(vanishing[1]), 0.0);
    EXPECT_FALSE(std::signbit(toDouble(vanishing[1])));
    EXPECT_TRUE(std::signbit(toDouble(vanishing[0])));

    // 2^-1074 x^4 - 2^200 x^2 with a leading coefficient at the bottom of the
    // doubles: x^2 (x^2 - 2^1274), roots -2^637, 0 (double), 2^637.
    const std::vector<RealRoot> wide = realRoots(poly({0.0, 0.0, -0x1p200, 0.0, 0x1p-1074}));
    ASSERT_EQ(wide.size(), 3U);
    EXPECT_EQ(compare(wide[0], -power(637)), Sign::zero);
    EXPECT_EQ(compare(wide[1], d(0.0)), Sign::zero);
    EXPECT_EQ(wide[1].multiplicity(), 2);
    EXPECT_EQ(toDouble(wide[2]), 0x1p637);
}

// ---------------------------------------------------------------------------
// Signs at roots and comparisons

TEST(PolynomialTest, SignOfAnotherPolynomialAtARoot) {
    const std::vector<RealRoot> roots = realRoots(squareMinus(2.0));
    ASSERT_EQ(roots.size(), 2U);
    const RealRoot& sqrt2 = roots[1];

    // A common root: x^4 - 4 = (x^2 - 2)(x^2 + 2).
    EXPECT_EQ(signAt(poly({-4.0, 0.0, 0.0, 0.0, 1.0}), sqrt2), Sign::zero);
    // Roots of x^2 - 2 -+ 2^-100, next to sqrt(2) by about 2^-102.
    EXPECT_EQ(signAt(squareMinus(d(2.0) + power(-100)), sqrt2), Sign::negative);
    EXPECT_EQ(signAt(squareMinus(d(2.0) - power(-100)), sqrt2), Sign::positive);
    // The doubles on each side of sqrt(2): 1.4142135623730951 is above it,
    // the one before is below.
    const double above = std::numbers::sqrt2;
    const double below = std::nextafter(above, 0.0);
    EXPECT_EQ(signAt(linear(above), sqrt2), Sign::negative);
    EXPECT_EQ(signAt(linear(below), sqrt2), Sign::positive);
    EXPECT_EQ(compare(sqrt2, d(above)), Sign::negative);
    EXPECT_EQ(compare(sqrt2, d(below)), Sign::positive);
    // At -sqrt(2), the odd polynomial x^3 is negative; a constant keeps its
    // sign; zero is zero.
    EXPECT_EQ(signAt(poly({0.0, 0.0, 0.0, 1.0}), roots[0]), Sign::negative);
    EXPECT_EQ(signAt(poly({-0.25}), roots[0]), Sign::negative);
    EXPECT_EQ(signAt(Polynomial(), roots[0]), Sign::zero);

    // With a negative leading coefficient and a root of multiplicity two:
    // -(x^2 - 2)^2; x^2 - 2 vanishes at sqrt(2), x - 2 is negative there.
    const std::vector<RealRoot> negative = realRoots(-(squareMinus(2.0) * squareMinus(2.0)));
    ASSERT_EQ(negative.size(), 2U);
    EXPECT_EQ(negative[1].multiplicity(), 2);
    EXPECT_EQ(signAt(squareMinus(2.0), negative[1]), Sign::zero);
    EXPECT_EQ(signAt(linear(2.0), negative[1]), Sign::negative);
    EXPECT_EQ(signAt(linear(1.0), negative[1]), Sign::positive);
}

TEST(PolynomialTest, EqualRootsOfDifferentPolynomials) {
    // p = x^2 - 2 and p * q with q = x^2 - 3: their roots +-sqrt(2) are the
    // same numbers.
    const Polynomial p = squareMinus(2.0);
    const std::vector<RealRoot> ofP = realRoots(p);
    const std::vector<RealRoot> ofProduct = realRoots(p * squareMinus(3.0));
    ASSERT_EQ(ofP.size(), 2U);
    ASSERT_EQ(ofProduct.size(), 4U);
    EXPECT_EQ(compare(ofP[0], ofProduct[1]), Sign::zero);
    EXPECT_EQ(compare(ofP[1], ofProduct[2]), Sign::zero);
    EXPECT_EQ(compare(ofProduct[2], ofP[1]), Sign::zero);
    EXPECT_EQ(compare(ofP[1], ofProduct[3]), Sign::negative);
    EXPECT_EQ(compare(ofProduct[3], ofP[1]), Sign::positive);
    EXPECT_EQ(compare(ofP[0], ofProduct[0]), Sign::positive);
    EXPECT_EQ(compare(ofP[1], ofProduct[1]), Sign::positive);

    // The same root written another way: 4 - 2x^2, and (x^2 - 2)^2.
    const std::vector<RealRoot> scaled = realRoots(poly({4.0, 0.0, -2.0}));
    const std::vector<RealRoot> squared = realRoots(p * p);
    ASSERT_EQ(scaled.size(), 2U);
    ASSERT_EQ(squared.size(), 2U);
    EXPECT_EQ(compare(scaled[1], ofP[1]), Sign::zero);
    EXPECT_EQ(compare(squared[1], scaled[1]), Sign::zero);
    EXPECT_EQ(compare(squared[0], scaled[1]), Sign::negative);

    // A root of a different polynomial 2^-60 away: sqrt(2) + 2^-60.
    const Dyadic shift = power(-60);
    const std::vector<RealRoot> shifted =
        realRoots(Polynomial({shift * shift - d(2.0), d(-2.0) * shift, d(1.0)}));
    ASSERT_EQ(shifted.size(), 2U);
    EXPECT_EQ(compare(ofP[1], shifted[1]), Sign::negative);
    EXPECT_EQ(compare(shifted[1], ofP[1]), Sign::positive);

    // An exact root against an isolated one: 1.5 and sqrt(2), sqrt(3). The
    // root of 2x - 3 is isolated in an interval of power-of-two ends, and
    // narrowing it meets 1.5 as a midpoint.
    const std::vector<RealRoot> half = realRoots(poly({-3.0, 2.0}));
    ASSERT_EQ(half.size(), 1U);
    const RealRoot exactHalf = narrowed(half[0], power(-10));
    EXPECT_TRUE(exactHalf.isExact());
    EXPECT_TRUE(same(exactHalf.lower(), d(1.5)));
    for (const RealRoot& root : {half[0], exactHalf}) {
        EXPECT_EQ(compare(root, ofP[1]), Sign::positive);
        EXPECT_EQ(compare(ofP[1], root), Sign::negative);
        EXPECT_EQ(compare(root, ofProduct[3]), Sign::negative);
        EXPECT_EQ(compare(ofProduct[3], root), Sign::positive);
        EXPECT_EQ(compare(root, d(1.5)), Sign::zero);
    }
    EXPECT_EQ(compare(half[0], exactHalf), Sign::zero);
    EXPECT_EQ(compare(exactHalf, half[0]), Sign::zero);
}

TEST(PolynomialTest, NarrowingReachesTheRequestedWidth) {
    const std::vector<RealRoot> roots = realRoots(squareMinus(2.0) * squareMinus(3.0));
    ASSERT_EQ(roots.size(), 4U);
    for (const std::int64_t exponent : {-10, -53, -80, -200}) {
        const Dyadic width = power(exponent);
        const RealRoot narrow = narrowed(roots[2], width);
        EXPECT_NE(compare(narrow.upper() - narrow.lower(), width), Sign::positive);
        // Still sqrt(2): lower^2 < 2 < upper^2, inside the first interval.
        EXPECT_EQ(compare(narrow.lower() * narrow.lower(), d(2.0)), Sign::negative);
        EXPECT_EQ(compare(narrow.upper() * narrow.upper(), d(2.0)), Sign::positive);
        EXPECT_NE(compare(roots[2].lower(), narrow.lower()), Sign::positive);
        EXPECT_NE(compare(narrow.upper(), roots[2].upper()), Sign::positive);
        EXPECT_EQ(compare(narrow, roots[2]), Sign::zero);
        expectWellFormed(narrow);
    }
    // An exact root stays as it is; a dyadic root may become exact.
    const std::vector<RealRoot> dyadicRoot = realRoots(linear(0.375) * linear(-7.0));
    ASSERT_EQ(dyadicRoot.size(), 2U);
    const RealRoot exact = narrowed(dyadicRoot[1], power(-70));
    EXPECT_EQ(compare(exact, d(0.375)), Sign::zero);
    EXPECT_NE(compare(exact.upper() - exact.lower(), power(-70)), Sign::positive);
}

// ---------------------------------------------------------------------------
// Generated cases

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

    // A double with 20 random bits in [-8, 8), so that coincidences happen.
    double coarse() {
        constexpr std::uint64_t kBits = 20;
        const auto value = static_cast<std::int64_t>(next() >> (64U - kBits));
        return std::ldexp(static_cast<double>(value - (std::int64_t{1} << (kBits - 1U))),
                          -static_cast<int>(kBits - 4U));
    }

private:
    std::uint64_t m_state;
};

TEST(PolynomialTest, GeneratedProductsOfKnownFactors) {
    Generator generator(0x5053330000000003ULL);
    for (int round = 0; round < 100; ++round) {
        // Four linear factors with roots drawn from a small set, so that some
        // repeat, times a nonzero dyadic.
        std::vector<double> chosen;
        Polynomial p = poly({generator.coarse() + 9.0});
        for (int i = 0; i < kMaxDegree; ++i) {
            const double root = (generator.next() % 3 == 0 && !chosen.empty())
                                    ? chosen[generator.next() % chosen.size()]
                                    : generator.coarse();
            chosen.push_back(root);
            p = p * linear(root);
        }
        std::vector<double> distinct = chosen;
        std::sort(distinct.begin(), distinct.end());
        distinct.erase(std::unique(distinct.begin(), distinct.end()), distinct.end());

        const std::vector<RealRoot> roots = realRoots(p);
        ASSERT_EQ(roots.size(), distinct.size());
        EXPECT_EQ(SturmSequence(p).countRoots(), distinct.size());
        for (std::size_t i = 0; i < roots.size(); ++i) {
            expectWellFormed(roots[i]);
            EXPECT_EQ(compare(roots[i], d(distinct[i])), Sign::zero);
            EXPECT_EQ(toDouble(roots[i]), distinct[i]);
            const auto multiplicity = std::count(chosen.begin(), chosen.end(), distinct[i]);
            EXPECT_EQ(roots[i].multiplicity(), static_cast<int>(multiplicity));
        }
    }
}

TEST(PolynomialTest, GeneratedSquareRoots) {
    Generator generator(0x5053330000000004ULL);
    for (int round = 0; round < 100; ++round) {
        // (x^2 - a)(x^2 - b) with a, b > 0: +-sqrt(a), +-sqrt(b).
        const double a = std::abs(generator.coarse()) + 0x1p-10;
        const double b = std::abs(generator.coarse()) + 0x1p-10;
        const Polynomial p = squareMinus(a) * squareMinus(b);
        const std::vector<RealRoot> roots = realRoots(p);
        const std::size_t expected = a == b ? 2 : 4;
        ASSERT_EQ(roots.size(), expected);
        const double small = std::sqrt(std::min(a, b));
        const double large = std::sqrt(std::max(a, b));
        expectRoundedNear(roots.front(), -large);
        expectRoundedNear(roots.back(), large);
        if (expected == 4) {
            expectRoundedNear(roots[1], -small);
            expectRoundedNear(roots[2], small);
        }
        for (std::size_t i = 0; i < roots.size(); ++i) {
            expectWellFormed(roots[i]);
            EXPECT_EQ(roots[i].multiplicity(), expected == 2 ? 2 : 1);
            // Each root is a root of exactly the factor it comes from, and the
            // comparisons agree both ways.
            const bool ofA = signAt(squareMinus(a), roots[i]) == Sign::zero;
            const bool ofB = signAt(squareMinus(b), roots[i]) == Sign::zero;
            EXPECT_TRUE(ofA || ofB);
            for (std::size_t j = 0; j < roots.size(); ++j) {
                Sign expectedOrder = Sign::zero;
                if (i < j) {
                    expectedOrder = Sign::negative;
                } else if (i > j) {
                    expectedOrder = Sign::positive;
                }
                EXPECT_EQ(compare(roots[i], roots[j]), expectedOrder);
            }
        }
    }
}

}  // namespace
