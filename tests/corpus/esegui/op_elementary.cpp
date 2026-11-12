// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "operations.hpp"
#include "plane_data.hpp"
#include "solid_space/robust/elementary.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <stdexcept>

namespace solid_space::corpus {
namespace {

Value tanOperation(const Value& input) {
    const double angle = readNumber(field(input, "angolo"), "angolo");
    const double result = robust::tan(angle);
    return Value::group({
        {"tangente", Value::number(result)},
        {"tangente_negativa", Value::boolean(std::signbit(result))},
    });
}

Value acosOperation(const Value& input) {
    const double x = readNumber(field(input, "x"), "x");
    const double result = robust::acos(x);
    return Value::group({
        {"angolo", Value::number(result)},
        {"angolo_negativo", Value::boolean(std::signbit(result))},
    });
}

Value hypotOperation(const Value& input) {
    const double x = readNumber(field(input, "x"), "x");
    const double y = readNumber(field(input, "y"), "y");
    const double result = robust::hypot(x, y);
    const auto bits = std::bit_cast<std::uint64_t>(result);
    for (const double signedX : {x, -x}) {
        for (const double signedY : {y, -y}) {
            if (std::bit_cast<std::uint64_t>(robust::hypot(signedX, signedY)) != bits ||
                std::bit_cast<std::uint64_t>(robust::hypot(signedY, signedX)) != bits) {
                throw std::runtime_error("hypot changed bits after swapping or negating inputs");
            }
        }
    }
    return Value::group({
        {"lunghezza", Value::number(result)},
        {"lunghezza_negativa", Value::boolean(std::signbit(result))},
    });
}

const Registration kTan("tan", tanOperation);
const Registration kAcos("acos", acosOperation);
const Registration kHypot("hypot", hypotOperation);

}  // namespace
}  // namespace solid_space::corpus
