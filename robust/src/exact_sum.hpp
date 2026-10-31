// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// The exact sign of a sum of products of two doubles, sum of +-x_i * y_i: the
// arithmetic under orient2d and TolerancePolicy::coincident (ADR 0009, point 4;
// ADR 0010). Internal to robust: nothing here is part of the public contract.
// geom2d has its own, larger arithmetic (geom2d/src/exact/); the duplication is
// a choice of the stage 0.2 plan, section 4.
//
// Why not floating-point expansions: the data go down to 2^-1074, and the
// product of two small doubles falls below the subnormals, so it is not a
// double any more. Here every term is kept exactly as an integer times a power
// of two.
//
// Representation. A finite double is m * 2^q with m an integer below 2^53 and
// q in [-1074, 971]. The product of two of them is (m1 * m2) * 2^(q1 + q2),
// with m1 * m2 below 2^106 and q1 + q2 in [-2148, 1942]. Every product is
// therefore an integer multiple of 2^-2148, and as a multiple of 2^-2148 it is
// below 2^(106 + 1942 + 2148) = 2^4196. Positive and negative products are
// added to two separate natural numbers, both counted in units of 2^-2148, and
// the sign is the comparison of the two: no borrows, and carries stop early.
//
// Memory. Each natural number is kLimbs = 66 limbs of 64 bits (4224 bits):
// 4196 bits for one product plus 28 bits of headroom, so up to kMaxTerms =
// 2^20 terms (well below 2^28) never overflow. The two numbers take 1056
// bytes inside the object: nothing is allocated, and every function is
// noexcept.
//
// Determinism. There is no floating-point arithmetic: the doubles are read
// through their bits (std::bit_cast), and the rest is integer arithmetic. All
// of it is in exact_sum.cpp (ADR 0008, point 5).

#include "solid_space/robust/predicates.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace solid_space::robust::exact {

/// A sum of products of two doubles, held exactly. It starts at zero.
class ExactSum {
public:
    using Limb = std::uint64_t;

    /// Limbs of each of the two natural numbers (see the header comment).
    static constexpr std::size_t kLimbs = 66;

    /// Most terms one sum may hold: their total stays below 2^4224.
    static constexpr std::size_t kMaxTerms = std::size_t{1} << 20U;

    /// Adds x * y. Precondition (asserted in debug): x and y are finite, and
    /// fewer than kMaxTerms terms were added before.
    void add(double x, double y) noexcept;

    /// Subtracts x * y. Same preconditions as add.
    void subtract(double x, double y) noexcept;

    /// The exact sign of the sum.
    [[nodiscard]] Sign sign() const noexcept;

private:
    void accumulate(double x, double y, bool negate) noexcept;

    std::array<Limb, kLimbs> m_positive{};
    std::array<Limb, kLimbs> m_negative{};
    std::size_t m_terms = 0;
};

}  // namespace solid_space::robust::exact
