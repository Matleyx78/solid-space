// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "exact_sum.hpp"

#include <bit>
#include <cassert>

namespace solid_space::robust::exact {
namespace {

using Limb = ExactSum::Limb;

constexpr unsigned kLimbBits = 64;
constexpr unsigned kFractionBits = 52;
constexpr Limb kFractionMask = (Limb{1} << kFractionBits) - 1;
constexpr unsigned kExponentMask = 0x7ff;
constexpr unsigned kSignShift = 63;

// A finite double is m * 2^q: the exponent of its least significant bit is
// q = e - 1075 for a normal number (biased exponent e, hidden bit added), and
// -1074 for a subnormal one (e = 0).
constexpr int kNormalExponentOffset = 1075;
constexpr int kSubnormalExponent = -1074;

// Products are counted in units of 2^-2148, the least significant bit of the
// product of two subnormals: the exponent q1 + q2 becomes a shift of
// q1 + q2 + 2148 bits, between 0 and 1942 + 2148 = 4090.
constexpr int kProductExponentOffset = 2148;
[[maybe_unused]] constexpr int kMaxShift = 4090;  // only in assertions

struct Decomposed {
    bool negative = false;
    Limb mantissa = 0;  // below 2^53
    int exponent = 0;   // of the least significant bit
};

Decomposed decompose(double value) noexcept {
    const auto bits = std::bit_cast<std::uint64_t>(value);
    const auto biased = static_cast<int>((bits >> kFractionBits) & kExponentMask);
    assert(static_cast<unsigned>(biased) != kExponentMask && "the value must be finite");
    Decomposed result;
    result.negative = (bits >> kSignShift) != 0;
    result.mantissa = bits & kFractionMask;
    if (biased == 0) {
        result.exponent = kSubnormalExponent;
    } else {
        result.mantissa |= Limb{1} << kFractionBits;
        result.exponent = biased - kNormalExponentOffset;
    }
    return result;
}

struct Wide {
    Limb high = 0;
    Limb low = 0;
};

// The full 128-bit product of two 64-bit numbers, from four 32-bit products
// (portable: MSVC has no 128-bit integer type).
Wide multiply(Limb a, Limb b) noexcept {
    constexpr unsigned kHalf = 32;
    constexpr Limb kHalfMask = 0xffffffffU;
    const Limb a0 = a & kHalfMask;
    const Limb a1 = a >> kHalf;
    const Limb b0 = b & kHalfMask;
    const Limb b1 = b >> kHalf;
    const Limb p00 = a0 * b0;
    const Limb p01 = a0 * b1;
    const Limb p10 = a1 * b0;
    const Limb p11 = a1 * b1;
    // Below 3 * 2^32: no overflow.
    const Limb middle = (p00 >> kHalf) + (p01 & kHalfMask) + (p10 & kHalfMask);
    Wide result;
    result.low = (p00 & kHalfMask) | (middle << kHalf);
    result.high = p11 + (p01 >> kHalf) + (p10 >> kHalf) + (middle >> kHalf);
    return result;
}

// target += value * 2^shift, with the carry propagated upwards.
void addShifted(std::array<Limb, ExactSum::kLimbs>& target, Wide value, int shift) noexcept {
    assert(shift >= 0 && shift <= kMaxShift);
    const auto index = static_cast<std::size_t>(shift) / kLimbBits;
    const auto bit = static_cast<unsigned>(shift) % kLimbBits;
    std::array<Limb, 3> words{};
    if (bit == 0) {
        words = {value.low, value.high, 0};
    } else {
        words = {value.low << bit, (value.low >> (kLimbBits - bit)) | (value.high << bit),
                 value.high >> (kLimbBits - bit)};
    }
    // index + 2 <= 4090 / 64 + 2 = 65 < kLimbs.
    Limb* limb = target.data() + index;
    Limb* const end = target.data() + target.size();
    Limb carry = 0;
    for (const Limb word : words) {
        const Limb sum = *limb + word;
        const Limb withCarry = sum + carry;
        carry = static_cast<Limb>(sum < word) + static_cast<Limb>(withCarry < sum);
        *limb = withCarry;
        ++limb;
    }
    for (; carry != 0 && limb != end; ++limb) {
        *limb += 1;
        carry = static_cast<Limb>(*limb == 0);
    }
    assert(carry == 0 && "the sum outgrew its fixed size");
}

}  // namespace

void ExactSum::add(double x, double y) noexcept {
    accumulate(x, y, false);
}

void ExactSum::subtract(double x, double y) noexcept {
    accumulate(x, y, true);
}

void ExactSum::accumulate(double x, double y, bool negate) noexcept {
    assert(m_terms < kMaxTerms);
    ++m_terms;
    const auto first = decompose(x);
    const auto second = decompose(y);
    if (first.mantissa == 0 || second.mantissa == 0) {
        return;  // a zero product, whatever the signs of the zeros
    }
    const bool negative = first.negative != second.negative ? !negate : negate;
    const auto shift = first.exponent + second.exponent + kProductExponentOffset;
    addShifted(negative ? m_negative : m_positive, multiply(first.mantissa, second.mantissa),
               shift);
}

Sign ExactSum::sign() const noexcept {
    // From the most significant limb down.
    auto negative = m_negative.rbegin();
    for (auto positive = m_positive.rbegin(); positive != m_positive.rend();
         ++positive, ++negative) {
        if (*positive != *negative) {
            return *positive > *negative ? Sign::positive : Sign::negative;
        }
    }
    return Sign::zero;
}

}  // namespace solid_space::robust::exact
