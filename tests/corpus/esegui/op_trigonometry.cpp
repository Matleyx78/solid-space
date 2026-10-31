// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Corpus operations of robust: sinCos and atan2 (stage 0.2, package P5;
// vocabulary in docs/tappe/0.2/P5-trigonometria.md). The "_negativo" keys
// carry the sign bit, which the exact comparison of numbers ignores.

#include "operations.hpp"
#include "plane_data.hpp"
#include "solid_space/robust/trigonometry.hpp"

#include <cmath>

namespace solid_space::corpus {
namespace {

Value sinCosOperation(const Value& input) {
    const auto angle = readNumber(field(input, "angolo"), "angolo");
    const auto result = robust::sinCos(angle);
    return Value::group({
        {"seno", Value::number(result.sine)},
        {"coseno", Value::number(result.cosine)},
        {"seno_negativo", Value::boolean(std::signbit(result.sine))},
    });
}

Value atan2Operation(const Value& input) {
    const auto y = readNumber(field(input, "y"), "y");
    const auto x = readNumber(field(input, "x"), "x");
    const auto angle = robust::atan2(y, x);
    return Value::group({
        {"angolo", Value::number(angle)},
        {"angolo_negativo", Value::boolean(std::signbit(angle))},
    });
}

const Registration kSinCos("sinCos", sinCosOperation);
const Registration kAtan2("atan2", atan2Operation);

}  // namespace
}  // namespace solid_space::corpus
