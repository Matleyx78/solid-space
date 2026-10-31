// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// The constants of sinCos and atan2 (trigonometry.cpp). Internal to robust:
// nothing here is part of the public contract. Only constants, no
// computation (ADR 0008, point 5).
//
// Every value comes from exact rational arithmetic, with the Python 3 script
// below (standard library only); it prints the literals of this file. pi is
// Machin's formula, 16 atan(1/5) - 4 atan(1/239), with atan from Euler's
// series; the polynomial coefficients are Taylor coefficients, 1/n! and
// 1/(2n+1); atan(j/8) is Euler's series again. Nothing is taken from another
// library. A pair {hi, lo} is a double-double: hi = RN(v), lo = RN(v - hi), so
// hi + lo is within 2^-106 |v| of v.
//
//   from fractions import Fraction as F
//   from math import factorial
//
//   def atan(t):  # Euler's series: term n is below t * (t^2 / (1 + t^2))^n <= 2^-n
//       q, term, total = t * t / (1 + t * t), t / (1 + t * t), F(0)
//       for n in range(400):
//           total, term = total + term, term * q * (2 * n + 2) / (2 * n + 3)
//       return total
//
//   def dd(v):  # hi = RN(v), lo = RN(v - hi)
//       hi = float(v)
//       return f"{{{hi.hex()}, {float(v - F(hi)).hex()}}}"
//
//   pi = 16 * atan(F(1, 5)) - 4 * atan(F(1, 239))  # Machin; error below 2^-390
//   sine = [F((-1) ** n, factorial(2 * n + 1)) for n in range(10)]
//   cosine = [F((-1) ** n, factorial(2 * n)) for n in range(11)]
//   arctangent = [F((-1) ** n, 2 * n + 1) for n in range(9)]
//   parts, rest = [], pi / 2
//   for i in range(4):  # 33 bits each: multiples of 2^-32, 2^-65, 2^-98, 2^-131
//       unit = F(1, 2 ** (32 + 33 * i))
//       parts.append(rest // unit * unit)
//       rest -= parts[-1]
//   parts.append(F(float(rest)))
//   print("kTwoOverPi =", float(2 / pi).hex())
//   print("kPiOverTwoParts =", ", ".join(float(p).hex() for p in parts))
//   print("kPiOverTwo =", dd(pi / 2))
//   print("kPi =", dd(pi))
//   print("kSineHead =", ", ".join(dd(c) for c in sine[:3]))
//   print("kSineTail =", ", ".join(float(c).hex() for c in sine[3:]))
//   print("kCosineHead =", ", ".join(dd(c) for c in cosine[:3]))
//   print("kCosineTail =", ", ".join(float(c).hex() for c in cosine[3:]))
//   print("kArctangentHead =", ", ".join(dd(c) for c in arctangent[:2]))
//   print("kArctangentTail =", ", ".join(float(c).hex() for c in arctangent[2:]))
//   print("kArctangentOfEighths =", ", ".join(dd(atan(F(j, 8))) for j in range(9)))

#include <array>
#include <numbers>

namespace solid_space::robust::trig {

/// An unevaluated sum hi + lo of two doubles, with |lo| <= ulp(hi) / 2.
struct DoubleDouble {
    double hi = 0.0;
    double lo = 0.0;
};

/// RN(2 / pi): only chooses the quadrant, so its error does not matter.
inline constexpr double kTwoOverPi = 0x1.45f306dc9c883p-1;

/// pi / 2 split for the argument reduction. The first four parts have at most
/// 33 significant bits (multiples of 2^-32, 2^-65, 2^-98 and 2^-131), so their
/// product by a quadrant number below 2^20 is exact; the fifth is the double
/// nearest to the rest. pi / 2 minus their sum is below 2^-187.
inline constexpr std::array<double, 5> kPiOverTwoParts = {
    0x1.921fb54400000p+0, 0x1.0b4611a600000p-34, 0x1.3198a2e000000p-69, 0x1.b839a24000000p-104,
    0x1.2049c1114cf99p-132};

inline constexpr DoubleDouble kPiOverTwo = {0x1.921fb54442d18p+0, 0x1.1a62633145c07p-54};
/// kPi.hi is the double the script prints, 0x1.921fb54442d18p+1, which is
/// std::numbers::pi.
inline constexpr DoubleDouble kPi = {std::numbers::pi, 0x1.1a62633145c07p-53};

/// sin(r) / r = sum over n of (-1)^n z^n / (2n+1)!, z = r^2: n = 0..2 as
/// double-doubles, n = 3..9 as doubles.
inline constexpr std::array<DoubleDouble, 3> kSineHead = {{
    {0x1.0000000000000p+0, 0x0.0p+0},
    {-0x1.5555555555555p-3, -0x1.5555555555555p-57},
    {0x1.1111111111111p-7, 0x1.1111111111111p-63},
}};
inline constexpr std::array<double, 7> kSineTail = {
    -0x1.a01a01a01a01ap-13, 0x1.71de3a556c734p-19, -0x1.ae64567f544e4p-26, 0x1.6124613a86d09p-33,
    -0x1.ae7f3e733b81fp-41, 0x1.952c77030ad4ap-49, -0x1.2f49b46814157p-57};

/// cos(r) = sum over n of (-1)^n z^n / (2n)!, z = r^2: n = 0..2 as
/// double-doubles, n = 3..10 as doubles.
inline constexpr std::array<DoubleDouble, 3> kCosineHead = {{
    {0x1.0000000000000p+0, 0x0.0p+0},
    {-0x1.0000000000000p-1, 0x0.0p+0},
    {0x1.5555555555555p-5, 0x1.5555555555555p-59},
}};
inline constexpr std::array<double, 8> kCosineTail = {
    -0x1.6c16c16c16c17p-10, 0x1.a01a01a01a01ap-16, -0x1.27e4fb7789f5cp-22, 0x1.1eed8eff8d898p-29,
    -0x1.93974a8c07c9dp-37, 0x1.ae7f3e733b81fp-45, -0x1.6827863b97d97p-53, 0x1.e542ba4020225p-62};

/// atan(u) / u = sum over n of (-1)^n w^n / (2n+1), w = u^2: n = 0..1 as
/// double-doubles, n = 2..8 as doubles.
inline constexpr std::array<DoubleDouble, 2> kArctangentHead = {{
    {0x1.0000000000000p+0, 0x0.0p+0},
    {-0x1.5555555555555p-2, -0x1.5555555555555p-56},
}};
inline constexpr std::array<double, 7> kArctangentTail = {
    0x1.999999999999ap-3, -0x1.2492492492492p-3, 0x1.c71c71c71c71cp-4, -0x1.745d1745d1746p-4,
    0x1.3b13b13b13b14p-4, -0x1.1111111111111p-4, 0x1.e1e1e1e1e1e1ep-5};

/// atan(j / 8) for j = 0..8.
inline constexpr std::array<DoubleDouble, 9> kArctangentOfEighths = {{
    {0x0.0p+0, 0x0.0p+0},
    {0x1.fd5ba9aac2f6ep-4, -0x1.cd37686760c17p-59},
    {0x1.f5b75f92c80ddp-3, 0x1.8ab6e3cf7afbdp-57},
    {0x1.6f61941e4def1p-2, -0x1.c63aae6f6e918p-56},
    {0x1.dac670561bb4fp-2, 0x1.a2b7f222f65e2p-56},
    {0x1.1e00babdefeb4p-1, -0x1.928df287a668fp-58},
    {0x1.4978fa3269ee1p-1, 0x1.2419a87f2a458p-56},
    {0x1.700a7c5784634p-1, -0x1.8c34d25aadef6p-56},
    {0x1.921fb54442d18p-1, 0x1.1a62633145c07p-55},
}};

}  // namespace solid_space::robust::trig
