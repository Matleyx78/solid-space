// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "dyadic.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace solid_space::geom2d::exact {

namespace {

using Limb = Natural::Limb;
using robust::Sign;

constexpr std::int64_t kLimbBits = 64;
constexpr Limb kTopBit = Limb{1} << 63U;

// IEEE 754 binary64: 52 stored fraction bits, exponent bias 1023, and the
// weight of the last bit of the smallest subnormal, 2^-1074.
constexpr int kFractionBits = 52;
constexpr Limb kFractionMask = (Limb{1} << kFractionBits) - 1;
constexpr Limb kImplicitBit = Limb{1} << kFractionBits;
constexpr Limb kExponentFieldMask = 0x7FF;
constexpr Limb kInfinityField = 0x7FF;
constexpr std::int64_t kSubnormalExponent = -1074;
// A double with biased exponent field f and significand s (53 bits, implicit
// bit included) is s * 2^(f - kExponentOffset).
constexpr std::int64_t kExponentOffset = 1075;
constexpr std::int64_t kMaxBiasedExponent = 2046;
// The largest finite double is below 2^(kLargestPower + 1).
constexpr std::int64_t kLargestPower = 1023;
// The rounding keeps 53 of the 64 bits of a window: 11 are dropped.
constexpr std::int64_t kNormalDroppedBits = kLimbBits - (kFractionBits + 1);

// Quotient bits computed before rounding: the quotient lies in
// [2^kQuotientShift - 1, 2^(kQuotientShift + 1)), at least 55 bits, which with
// the remainder as sticky bit is enough for a correctly rounded double.
constexpr std::int64_t kQuotientShift = 56;

struct WideProduct {
    Limb high;
    Limb low;
};

// The 128-bit product of two limbs, with 32-bit halves so that it is the same
// code on every compiler (MSVC has no 128-bit integer type).
WideProduct multiplyLimbs(Limb a, Limb b) noexcept {
    constexpr Limb kHalfMask = 0xFFFFFFFFULL;
    constexpr unsigned kHalfBits = 32;
    const Limb a0 = a & kHalfMask;
    const Limb a1 = a >> kHalfBits;
    const Limb b0 = b & kHalfMask;
    const Limb b1 = b >> kHalfBits;
    const Limb p00 = a0 * b0;
    const Limb p01 = a0 * b1;
    const Limb p10 = a1 * b0;
    const Limb p11 = a1 * b1;
    const Limb middle = (p00 >> kHalfBits) + (p01 & kHalfMask) + (p10 & kHalfMask);
    return {p11 + (p01 >> kHalfBits) + (p10 >> kHalfBits) + (middle >> kHalfBits),
            (middle << kHalfBits) | (p00 & kHalfMask)};
}

std::size_t toSize(std::int64_t value) noexcept {
    assert(value >= 0);
    return static_cast<std::size_t>(value);
}

std::int64_t toSigned(std::size_t value) noexcept {
    return static_cast<std::int64_t>(value);
}

unsigned toShift(std::int64_t value) noexcept {
    assert(value >= 0 && value < kLimbBits);
    return static_cast<unsigned>(value);
}

std::int64_t bitLength(const Natural& x) noexcept {
    if (x.isZero()) {
        return 0;
    }
    const Limb top = x.data()[x.size() - 1];
    return (toSigned(x.size()) - 1) * kLimbBits + (kLimbBits - std::countl_zero(top));
}

std::int64_t trailingZeros(const Natural& x) noexcept {
    assert(!x.isZero());
    const Limb* limbs = x.data();
    std::int64_t zeros = 0;
    std::size_t index = 0;
    while (limbs[index] == 0) {
        zeros += kLimbBits;
        ++index;
    }
    return zeros + std::countr_zero(limbs[index]);
}

// The limb at `index` of the number x, or zero beyond its limbs.
Limb limbOrZero(const Natural& x, std::int64_t index) noexcept {
    if (index < 0 || index >= toSigned(x.size())) {
        return 0;
    }
    return x.data()[index];
}

// The 64 bits of x * 2^shift that start at bit `limbIndex * 64`: the limb of
// a shifted number, read without building it.
Limb shiftedLimb(const Natural& x, std::int64_t shift, std::int64_t limbIndex) noexcept {
    const std::int64_t whole = shift / kLimbBits;
    const std::int64_t part = shift % kLimbBits;
    const Limb upper = limbOrZero(x, limbIndex - whole);
    if (part == 0) {
        return upper;
    }
    const Limb lower = limbOrZero(x, limbIndex - whole - 1);
    return (upper << toShift(part)) | (lower >> toShift(kLimbBits - part));
}

// The 64 bits of x that start at bit `start` (bits beyond the top are zero).
Limb windowAt(const Natural& x, std::int64_t start) noexcept {
    assert(start >= 0);
    const std::int64_t index = start / kLimbBits;
    const std::int64_t part = start % kLimbBits;
    Limb window = limbOrZero(x, index) >> toShift(part);
    if (part != 0) {
        window |= limbOrZero(x, index + 1) << toShift(kLimbBits - part);
    }
    return window;
}

// Whether any bit of x below bit `count` is set.
bool hasBitsBelow(const Natural& x, std::int64_t count) noexcept {
    const Limb* limbs = x.data();
    const std::int64_t whole = std::min(count / kLimbBits, toSigned(x.size()));
    for (std::int64_t i = 0; i < whole; ++i) {
        if (limbs[i] != 0) {
            return true;
        }
    }
    const std::int64_t part = count % kLimbBits;
    if (part == 0 || whole >= toSigned(x.size())) {
        return false;
    }
    return (limbs[whole] & ((Limb{1} << toShift(part)) - 1)) != 0;
}

// The sign of a * 2^shiftA - b * 2^shiftB, for shifts that are not negative.
Sign compareShifted(const Natural& a, std::int64_t shiftA, const Natural& b,
                    std::int64_t shiftB) noexcept {
    const std::int64_t lengthA = a.isZero() ? 0 : bitLength(a) + shiftA;
    const std::int64_t lengthB = b.isZero() ? 0 : bitLength(b) + shiftB;
    if (lengthA != lengthB) {
        return lengthA > lengthB ? Sign::positive : Sign::negative;
    }
    if (lengthA == 0) {
        return Sign::zero;
    }
    const std::int64_t limbs = (lengthA + kLimbBits - 1) / kLimbBits;
    for (std::int64_t i = limbs - 1; i >= 0; --i) {
        const Limb limbA = shiftedLimb(a, shiftA, i);
        const Limb limbB = shiftedLimb(b, shiftB, i);
        if (limbA != limbB) {
            return limbA > limbB ? Sign::positive : Sign::negative;
        }
    }
    return Sign::zero;
}

Sign compareNaturals(const Natural& a, const Natural& b) noexcept {
    return compareShifted(a, 0, b, 0);
}

Natural shiftLeft(const Natural& x, std::int64_t bits) {
    assert(bits >= 0);
    Natural result;
    if (x.isZero()) {
        return result;
    }
    const std::int64_t whole = bits / kLimbBits;
    const std::int64_t part = bits % kLimbBits;
    const std::size_t size = x.size();
    const Limb* source = x.data();
    Limb* target = result.assignZeros(size + toSize(whole) + 1);
    for (std::size_t i = 0; i < size; ++i) {
        const std::size_t to = i + toSize(whole);
        target[to] |= source[i] << toShift(part);
        if (part != 0) {
            target[to + 1] |= source[i] >> toShift(kLimbBits - part);
        }
    }
    result.trim();
    return result;
}

// floor(x / 2^bits).
Natural shiftRight(const Natural& x, std::int64_t bits) {
    assert(bits >= 0);
    Natural result;
    const std::int64_t whole = bits / kLimbBits;
    if (whole >= toSigned(x.size())) {
        return result;
    }
    const std::size_t size = x.size() - toSize(whole);
    Limb* target = result.assignZeros(size);
    for (std::size_t i = 0; i < size; ++i) {
        target[i] = windowAt(x, bits + toSigned(i) * kLimbBits);
    }
    result.trim();
    return result;
}

Natural add(const Natural& a, const Natural& b) {
    const Natural& longer = a.size() >= b.size() ? a : b;
    const Natural& shorter = a.size() >= b.size() ? b : a;
    Natural result;
    Limb* target = result.assignZeros(longer.size() + 1);
    Limb carry = 0;
    for (std::size_t i = 0; i < longer.size(); ++i) {
        const Limb x = longer.data()[i];
        const Limb y = i < shorter.size() ? shorter.data()[i] : 0;
        const Limb partial = x + y;
        const Limb sum = partial + carry;
        carry = (partial < x ? 1U : 0U) + (sum < partial ? 1U : 0U);
        target[i] = sum;
    }
    target[longer.size()] = carry;
    result.trim();
    return result;
}

// a - b. Precondition: a >= b.
Natural subtract(const Natural& a, const Natural& b) {
    assert(compareNaturals(a, b) != Sign::negative);
    Natural result;
    Limb* target = result.assignZeros(a.size());
    Limb borrow = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const Limb x = a.data()[i];
        const Limb y = i < b.size() ? b.data()[i] : 0;
        const Limb partial = x - y;
        const Limb difference = partial - borrow;
        borrow = (x < y ? 1U : 0U) + (partial < borrow ? 1U : 0U);
        target[i] = difference;
    }
    assert(borrow == 0);
    result.trim();
    return result;
}

Natural multiply(const Natural& a, const Natural& b) {
    Natural result;
    if (a.isZero() || b.isZero()) {
        return result;
    }
    Limb* target = result.assignZeros(a.size() + b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        Limb carry = 0;
        for (std::size_t j = 0; j < b.size(); ++j) {
            const WideProduct product = multiplyLimbs(a.data()[i], b.data()[j]);
            const Limb partial = product.low + target[i + j];
            const Limb sum = partial + carry;
            // high <= 2^64 - 2, so adding the two carries cannot overflow.
            carry = product.high + (partial < product.low ? 1U : 0U) + (sum < partial ? 1U : 0U);
            target[i + j] = sum;
        }
        target[i + b.size()] = carry;
    }
    result.trim();
    return result;
}

Natural powerOfTwo(std::int64_t exponent) {
    return shiftLeft(Natural{1}, exponent);
}

// floor(sqrt(x)), digit by digit: only shifts, sums and comparisons.
Natural floorSquareRoot(const Natural& x) {
    Natural remainder = x;
    Natural root;
    if (x.isZero()) {
        return root;
    }
    const std::int64_t top = bitLength(x) - 1;
    for (std::int64_t position = top - top % 2; position >= 0; position -= 2) {
        const Natural bit = powerOfTwo(position);
        const Natural trial = add(root, bit);
        if (compareNaturals(remainder, trial) != Sign::negative) {
            remainder = subtract(remainder, trial);
            root = add(shiftRight(root, 1), bit);
        } else {
            root = shiftRight(root, 1);
        }
    }
    return root;
}

double signedZero(bool negative) noexcept {
    return negative ? -0.0 : 0.0;
}

// The nearest double to (window + f) * 2^exponent, where the top bit of window
// is set, f is in [0, 1), and f > 0 exactly when `sticky`. Ties to even.
double roundToDouble(bool negative, Limb window, std::int64_t exponent, bool sticky) noexcept {
    assert((window & kTopBit) != 0);
    const Limb signBit = negative ? kTopBit : 0;
    const Limb infinityBits = signBit | (kInfinityField << kFractionBits);
    // The value lies in [2^top, 2^(top + 1)).
    const std::int64_t top = exponent + kLimbBits - 1;
    if (top > kLargestPower) {
        return std::bit_cast<double>(infinityBits);
    }
    // Bits of the window below the last bit kept: 11 for a normal result, more
    // when the last bit kept is the one of the subnormals, 2^-1074.
    const std::int64_t dropped = std::max(kNormalDroppedBits, kSubnormalExponent - exponent);
    if (dropped > kLimbBits) {
        // Below 2^-1075, half the smallest subnormal.
        return signedZero(negative);
    }
    Limb kept = 0;
    if (dropped == kLimbBits) {
        // The value is in [2^-1075, 2^-1074): the window is all rounding bits.
        const bool aboveHalf = window > kTopBit || (window == kTopBit && sticky);
        kept = aboveHalf ? 1 : 0;
    } else {
        const unsigned shift = toShift(dropped);
        kept = window >> shift;
        const Limb rest = window & ((Limb{1} << shift) - 1);
        const Limb half = Limb{1} << (shift - 1);
        const bool roundUp = rest > half || (rest == half && (sticky || (kept & 1U) != 0));
        kept += roundUp ? 1U : 0U;
    }
    std::int64_t scale = exponent + dropped;
    if (kept == (kImplicitBit << 1U)) {
        kept = kImplicitBit;
        ++scale;
    }
    if (kept < kImplicitBit) {
        // A subnormal, or zero: the scale is the one of the subnormals.
        assert(scale == kSubnormalExponent);
        return std::bit_cast<double>(signBit | kept);
    }
    const std::int64_t biased = scale + kExponentOffset;
    if (biased > kMaxBiasedExponent) {
        return std::bit_cast<double>(infinityBits);
    }
    return std::bit_cast<double>(signBit | (static_cast<Limb>(biased) << kFractionBits) |
                                 (kept & kFractionMask));
}

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

}  // namespace

// --- Natural ---------------------------------------------------------------

Natural::Natural(Limb value) noexcept : m_size(value == 0 ? 0 : 1), m_inline{value} {}

const Natural::Limb* Natural::data() const noexcept {
    return m_heap.empty() ? m_inline.data() : m_heap.data();
}

Natural::Limb* Natural::mutableData() noexcept {
    return m_heap.empty() ? m_inline.data() : m_heap.data();
}

Natural::Limb* Natural::assignZeros(std::size_t size) {
    if (size <= kInlineLimbs) {
        // clear() keeps the capacity: shrinking never frees or allocates.
        m_heap.clear();
        m_inline.fill(0);
    } else {
        m_heap.assign(size, 0);
    }
    m_size = size;
    return mutableData();
}

void Natural::trim() noexcept {
    const Limb* limbs = data();
    while (m_size > 0 && limbs[m_size - 1] == 0) {
        --m_size;
    }
}

// --- Dyadic ----------------------------------------------------------------

Dyadic::Dyadic(Natural magnitude, std::int64_t exponent, bool negative)
    : m_magnitude(std::move(magnitude)), m_exponent(exponent), m_negative(negative) {
    normalize();
}

Dyadic::Dyadic(double value) noexcept {
    const auto bits = std::bit_cast<Limb>(value);
    const Limb field = (bits >> kFractionBits) & kExponentFieldMask;
    assert(field != kInfinityField && "Dyadic needs a finite double");
    m_negative = (bits & kTopBit) != 0;
    Limb significand = bits & kFractionMask;
    std::int64_t exponent = kSubnormalExponent;
    if (field != 0) {
        significand |= kImplicitBit;
        exponent = static_cast<std::int64_t>(field) - kExponentOffset;
    }
    if (significand != 0) {
        // One limb: normalize here, with no call that might allocate.
        const int zeros = std::countr_zero(significand);
        m_magnitude = Natural{significand >> static_cast<unsigned>(zeros)};
        m_exponent = exponent + zeros;
    }
}

Dyadic Dyadic::fromInteger(std::int64_t value) noexcept {
    Dyadic result;
    if (value == 0) {
        return result;
    }
    result.m_negative = value < 0;
    // The magnitude of the most negative value does not fit in std::int64_t,
    // but it does in a limb: negate in unsigned arithmetic.
    Limb magnitude = static_cast<Limb>(value);
    if (value < 0) {
        magnitude = ~magnitude + 1;
    }
    const int zeros = std::countr_zero(magnitude);
    result.m_magnitude = Natural{magnitude >> static_cast<unsigned>(zeros)};
    result.m_exponent = zeros;
    return result;
}

void Dyadic::normalize() {
    if (m_magnitude.isZero()) {
        m_exponent = 0;
        return;
    }
    const std::int64_t zeros = trailingZeros(m_magnitude);
    if (zeros != 0) {
        m_magnitude = shiftRight(m_magnitude, zeros);
        m_exponent += zeros;
    }
}

Sign Dyadic::sign() const noexcept {
    if (isZero()) {
        return Sign::zero;
    }
    return m_negative ? Sign::negative : Sign::positive;
}

double Dyadic::toDouble() const noexcept {
    if (isZero()) {
        return signedZero(m_negative);
    }
    const std::int64_t length = bitLength(m_magnitude);
    if (length <= kLimbBits) {
        const Limb window = m_magnitude.data()[0] << toShift(kLimbBits - length);
        return roundToDouble(m_negative, window, m_exponent - (kLimbBits - length), false);
    }
    const std::int64_t start = length - kLimbBits;
    return roundToDouble(m_negative, windowAt(m_magnitude, start), m_exponent + start,
                         hasBitsBelow(m_magnitude, start));
}

Dyadic Dyadic::timesPowerOfTwo(std::int64_t power) const {
    Dyadic result = *this;
    if (!result.isZero()) {
        result.m_exponent += power;
    }
    return result;
}

Dyadic Dyadic::abs() const {
    Dyadic result = *this;
    result.m_negative = false;
    return result;
}

Dyadic operator+(const Dyadic& a, const Dyadic& b) {
    if (a.isZero() && b.isZero()) {
        // IEEE 754: -0 + -0 is -0, every other sum of zeros is +0.
        Dyadic result;
        result.m_negative = a.m_negative && b.m_negative;
        return result;
    }
    if (a.isZero()) {
        return b;
    }
    if (b.isZero()) {
        return a;
    }
    const std::int64_t exponent = std::min(a.m_exponent, b.m_exponent);
    const Natural alignedA = shiftLeft(a.m_magnitude, a.m_exponent - exponent);
    const Natural alignedB = shiftLeft(b.m_magnitude, b.m_exponent - exponent);
    if (a.m_negative == b.m_negative) {
        return Dyadic{add(alignedA, alignedB), exponent, a.m_negative};
    }
    switch (compareNaturals(alignedA, alignedB)) {
        case Sign::positive:
            return Dyadic{subtract(alignedA, alignedB), exponent, a.m_negative};
        case Sign::negative:
            return Dyadic{subtract(alignedB, alignedA), exponent, b.m_negative};
        case Sign::zero:
            break;
    }
    // An exact zero sum is +0 in round-to-nearest.
    return Dyadic{};
}

Dyadic operator-(const Dyadic& a) {
    Dyadic result = a;
    result.m_negative = !a.m_negative;
    return result;
}

Dyadic operator-(const Dyadic& a, const Dyadic& b) {
    return a + (-b);
}

Dyadic operator*(const Dyadic& a, const Dyadic& b) {
    const bool negative = a.m_negative != b.m_negative;
    if (a.isZero() || b.isZero()) {
        Dyadic result;
        result.m_negative = negative;
        return result;
    }
    // The product of two odd numbers is odd: it is already normalized.
    return Dyadic{multiply(a.m_magnitude, b.m_magnitude), a.m_exponent + b.m_exponent, negative};
}

Dyadic& Dyadic::operator+=(const Dyadic& other) {
    *this = *this + other;
    return *this;
}

Dyadic& Dyadic::operator-=(const Dyadic& other) {
    *this = *this - other;
    return *this;
}

Dyadic& Dyadic::operator*=(const Dyadic& other) {
    *this = *this * other;
    return *this;
}

Sign compare(const Dyadic& a, const Dyadic& b) noexcept {
    const Sign signA = a.sign();
    const Sign signB = b.sign();
    if (signA != signB) {
        return signA > signB ? Sign::positive : Sign::negative;
    }
    if (signA == Sign::zero) {
        return Sign::zero;
    }
    const std::int64_t exponent = std::min(a.m_exponent, b.m_exponent);
    const Sign magnitudes = compareShifted(a.m_magnitude, a.m_exponent - exponent, b.m_magnitude,
                                           b.m_exponent - exponent);
    return signA == Sign::positive ? magnitudes : negated(magnitudes);
}

double quotientToDouble(const Dyadic& numerator, const Dyadic& denominator) {
    assert(!denominator.isZero() && "quotientToDouble needs a denominator that is not zero");
    const bool negative = numerator.m_negative != denominator.m_negative;
    if (numerator.isZero()) {
        return signedZero(negative);
    }
    // Scale so that the integer quotient q = floor(n * 2^shift / d) has 56 or
    // 57 bits: n / d lies in (2^(ln - ld - 1), 2^(ln - ld + 1)).
    const std::int64_t shift =
        kQuotientShift - (bitLength(numerator.m_magnitude) - bitLength(denominator.m_magnitude));
    Natural remainder =
        shift >= 0 ? shiftLeft(numerator.m_magnitude, shift) : numerator.m_magnitude;
    const Natural divisor =
        shift >= 0 ? denominator.m_magnitude : shiftLeft(denominator.m_magnitude, -shift);
    // Binary long division: the quotient has at most kQuotientShift + 1 bits.
    Limb quotient = 0;
    for (std::int64_t bit = kQuotientShift; bit >= 0; --bit) {
        const Natural trial = shiftLeft(divisor, bit);
        if (compareNaturals(remainder, trial) != Sign::negative) {
            remainder = subtract(remainder, trial);
            quotient |= Limb{1} << toShift(bit);
        }
    }
    assert(quotient != 0);
    const int leading = std::countl_zero(quotient);
    return roundToDouble(negative, quotient << static_cast<unsigned>(leading),
                         numerator.m_exponent - denominator.m_exponent - shift - leading,
                         !remainder.isZero());
}

Dyadic floorSqrt(const Dyadic& value, std::int64_t bits) {
    assert(value.sign() != Sign::negative && "floorSqrt needs a value that is not negative");
    assert(bits >= 2);
    if (value.isZero()) {
        return Dyadic{};
    }
    // value = m * 2^e. Write it as M * 2^(2j), with M = m * 2^s at least 2 * bits
    // bits long: then sqrt(value) = sqrt(M) * 2^j, and r = floor(sqrt(M)) has at
    // least `bits` bits, so sqrt(M) - r < 1 <= r * 2^(1 - bits).
    std::int64_t extra = std::max<std::int64_t>(0, 2 * bits - bitLength(value.m_magnitude));
    if ((value.m_exponent - extra) % 2 != 0) {
        ++extra;
    }
    const Natural root = floorSquareRoot(shiftLeft(value.m_magnitude, extra));
    return Dyadic{root, (value.m_exponent - extra) / 2, false};
}

}  // namespace solid_space::geom2d::exact
