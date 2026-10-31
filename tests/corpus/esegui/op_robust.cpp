// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Corpus operations of robust: orient2d and the tolerance rule (stage 0.2,
// package P4; vocabulary in tests/corpus/README.md, "Dati del piano").

#include "operations.hpp"
#include "plane_data.hpp"
#include "solid_space/robust/predicates.hpp"
#include "solid_space/robust/tolerance.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace solid_space::corpus {
namespace {

Value writeSign(robust::Sign sign) {
    switch (sign) {
        case robust::Sign::negative:
            return Value::name("negative");
        case robust::Sign::zero:
            return Value::name("zero");
        case robust::Sign::positive:
            return Value::name("positive");
    }
    throw std::runtime_error("Sign: unknown enumerator");
}

double unitOf(const Value& input) {
    return readNumber(field(input, "unita_in_metri"), "unita_in_metri");
}

// The operations other than forUnit need a valid rule: a case that gives an
// invalid unit to them is malformed.
robust::TolerancePolicy policyOf(const Value& input) {
    auto policy = robust::TolerancePolicy::forUnit(unitOf(input));
    if (!policy.hasValue()) {
        throw std::runtime_error("unita_in_metri: invalid unit: " + policy.diagnosis().message);
    }
    return std::move(policy).value();
}

Value orient2dOperation(const Value& input) {
    const auto a = readPoint(field(input, "a"), "a");
    const auto b = readPoint(field(input, "b"), "b");
    const auto c = readPoint(field(input, "c"), "c");
    return Value::group({{"segno", writeSign(robust::orient2d(a, b, c))}});
}

Value forUnitOperation(const Value& input) {
    const auto policy = robust::TolerancePolicy::forUnit(unitOf(input));
    if (!policy.hasValue()) {
        return writeDiagnosis(policy.diagnosis());
    }
    return Value::group({{"lunghezza_minima", Value::number(policy.value().floorLength())}});
}

Value epsilonOperation(const Value& input) {
    const auto policy = policyOf(input);
    const auto magnitude = readNumber(field(input, "grandezza"), "grandezza");
    return Value::group({{"epsilon", Value::number(policy.epsilon(magnitude))}});
}

Value coincidentOperation(const Value& input) {
    const auto policy = policyOf(input);
    const auto a = readPoint(field(input, "a"), "a");
    const auto b = readPoint(field(input, "b"), "b");
    return Value::group({{"coincidono", Value::boolean(policy.coincident(a, b))}});
}

Value negligibleOperation(const Value& input) {
    const auto policy = policyOf(input);
    const auto length = readNumber(field(input, "lunghezza"), "lunghezza");
    const auto magnitude = readNumber(field(input, "grandezza"), "grandezza");
    return Value::group({{"trascurabile", Value::boolean(policy.negligible(length, magnitude))}});
}

const Registration kOrient2d("orient2d", orient2dOperation);
const Registration kForUnit("forUnit", forUnitOperation);
const Registration kEpsilon("epsilon", epsilonOperation);
const Registration kCoincident("coincident", coincidentOperation);
const Registration kNegligible("negligible", negligibleOperation);

}  // namespace
}  // namespace solid_space::corpus
