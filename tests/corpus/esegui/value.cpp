// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "value.hpp"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cmath>
#include <limits>

namespace solid_space::corpus {

Value::Value(Kind kind) : m_kind(kind) {}

Value Value::boolean(bool value) {
    Value result(Kind::boolean);
    result.m_boolean = value;
    return result;
}

Value Value::number(double value) {
    Value result(Kind::number);
    result.m_number = value;
    return result;
}

Value Value::exact(std::string value, std::optional<double> representable) {
    Value result(Kind::exact);
    result.m_string = std::move(value);
    result.m_representable = representable;
    return result;
}

Value Value::integer(std::int64_t value) {
    const auto number = static_cast<double>(value);
    // Binary64 has 53 significant bits. Large integers need enough trailing
    // zero bits; do not convert a rounded double back to int64 (it may overflow).
    auto magnitude = static_cast<std::uint64_t>(value);
    if (value < 0) {
        magnitude = std::uint64_t{0} - magnitude;
    }
    const auto width = std::bit_width(magnitude);
    const bool fits = width <= 53 || std::countr_zero(magnitude) >= static_cast<int>(width - 53);
    return exact(std::to_string(value), fits ? std::optional<double>(number) : std::nullopt);
}

Value Value::approximate(double value, double tolerance) {
    Value result(Kind::approximate);
    result.m_number = value;
    result.m_tolerance = tolerance;
    return result;
}

Value Value::name(std::string value) {
    Value result(Kind::name);
    result.m_string = std::move(value);
    return result;
}

Value Value::text(std::string value) {
    Value result(Kind::text);
    result.m_string = std::move(value);
    return result;
}

Value Value::list(List value) {
    Value result(Kind::list);
    result.m_list = std::move(value);
    return result;
}

Value Value::group(Group value) {
    Value result(Kind::group);
    result.m_group = std::move(value);
    return result;
}

Value::Kind Value::kind() const {
    return m_kind;
}

bool Value::asBoolean() const {
    assert(m_kind == Kind::boolean);
    return m_boolean;
}

double Value::asNumber() const {
    assert(m_kind == Kind::number || m_kind == Kind::approximate);
    return m_number;
}

double Value::tolerance() const {
    assert(m_kind == Kind::approximate);
    return m_tolerance;
}

std::optional<double> Value::representable() const {
    assert(m_kind == Kind::exact);
    return m_representable;
}

const std::string& Value::asString() const {
    assert(m_kind == Kind::exact || m_kind == Kind::name || m_kind == Kind::text);
    return m_string;
}

const Value::List& Value::asList() const {
    assert(m_kind == Kind::list);
    return m_list;
}

const Value::Group& Value::asGroup() const {
    assert(m_kind == Kind::group);
    return m_group;
}

const Value* Value::find(std::string_view key) const {
    for (const auto& [name, value] : asGroup()) {
        if (name == key) {
            return &value;
        }
    }
    return nullptr;
}

namespace {

std::string hexNumber(double number) {
    // Encode the binary64 fields directly: no locale, stream formatting or
    // platform-specific spelling of subnormals and signed zeros.
    static_assert(std::numeric_limits<double>::is_iec559 && sizeof(double) == 8);
    constexpr std::uint64_t kFractionMask = (std::uint64_t{1} << 52) - 1;
    constexpr std::string_view kDigits = "0123456789abcdef";
    const auto bits = std::bit_cast<std::uint64_t>(number);
    const auto exponent = (bits >> 52) & 0x7ff;
    const auto fraction = bits & kFractionMask;
    std::string result = (bits >> 63) != 0 ? "-" : "";
    if (exponent == 0x7ff) {
        return fraction == 0 ? result + "inf" : "nan";
    }
    result += exponent == 0 ? "0x0." : "0x1.";
    for (int shift = 48; shift >= 0; shift -= 4) {
        result += kDigits[static_cast<std::size_t>((fraction >> shift) & 0xf)];
    }
    int power = static_cast<int>(exponent) - 1023;
    if (exponent == 0) {
        power = fraction == 0 ? 0 : -1022;
    }
    result += "p";
    if (power >= 0) {
        result += "+";
    }
    return result + std::to_string(power);
}

std::string quoted(std::string_view text) {
    constexpr std::string_view kDigits = "0123456789abcdef";
    std::string result = "\"";
    for (const char raw : text) {
        const auto byte = static_cast<unsigned char>(raw);
        if (byte < 32 || byte >= 127 || byte == '"' || byte == '\\') {
            result += "\\x";
            result += kDigits[byte >> 4];
            result += kDigits[byte & 0xf];
        } else {
            result += raw;
        }
    }
    return result + "\"";
}

void compareAt(const Value& expected, const Value& actual, const std::string& path,
               std::vector<std::string>& differences) {
    using Kind = Value::Kind;
    bool equal = false;
    if (expected.kind() == Kind::exact && actual.kind() == Kind::number) {
        const auto number = expected.representable();
        equal = number.has_value() && *number == actual.asNumber();
    } else if (expected.kind() == Kind::approximate && actual.kind() == Kind::number) {
        equal = std::isfinite(actual.asNumber()) &&
                std::abs(actual.asNumber() - expected.asNumber()) <= expected.tolerance();
    } else if (expected.kind() == actual.kind()) {
        switch (expected.kind()) {
            case Kind::boolean:
                equal = expected.asBoolean() == actual.asBoolean();
                break;
            case Kind::number:
                equal = expected.asNumber() == actual.asNumber();
                break;
            case Kind::exact:
            case Kind::name:
            case Kind::text:
                equal = expected.asString() == actual.asString();
                break;
            case Kind::approximate:
                equal = expected.asNumber() == actual.asNumber() &&
                        expected.tolerance() == actual.tolerance();
                break;
            case Kind::list: {
                equal = expected.asList().size() == actual.asList().size();
                const auto count = std::min(expected.asList().size(), actual.asList().size());
                for (std::size_t i = 0; i < count; ++i) {
                    compareAt(expected.asList()[i], actual.asList()[i],
                              path + "[" + std::to_string(i) + "]", differences);
                }
                break;
            }
            case Kind::group:
                for (const auto& [key, value] : expected.asGroup()) {
                    const auto* found = actual.find(key);
                    const auto child = path.empty() ? key : path + "." + key;
                    if (found != nullptr) {
                        compareAt(value, *found, child, differences);
                    } else {
                        differences.push_back(child + ": missing key");
                    }
                }
                return;
        }
    }
    if (!equal) {
        differences.push_back((path.empty() ? "(root)" : path) + ": expected " +
                              canonicalText(expected) + ", actual " + canonicalText(actual));
    }
}

}  // namespace

std::vector<std::string> compare(const Value& expected, const Value& actual) {
    std::vector<std::string> differences;
    compareAt(expected, actual, "", differences);
    return differences;
}

std::string canonicalText(const Value& actual) {
    using Kind = Value::Kind;
    switch (actual.kind()) {
        case Kind::boolean:
            return actual.asBoolean() ? "true" : "false";
        case Kind::number:
            return hexNumber(actual.asNumber());
        case Kind::exact:
            return "exact(" + quoted(actual.asString()) + ")";
        case Kind::approximate:
            return "approx(" + hexNumber(actual.asNumber()) + "," + hexNumber(actual.tolerance()) +
                   ")";
        case Kind::name:
            return "name(" + quoted(actual.asString()) + ")";
        case Kind::text:
            return quoted(actual.asString());
        case Kind::list: {
            std::string result = "[";
            for (const auto& value : actual.asList()) {
                if (result.size() > 1) {
                    result += ",";
                }
                result += canonicalText(value);
            }
            return result + "]";
        }
        case Kind::group: {
            std::string result = "{";
            for (const auto& [key, value] : actual.asGroup()) {
                if (result.size() > 1) {
                    result += ",";
                }
                result += quoted(key) + ":" + canonicalText(value);
            }
            return result + "}";
        }
    }
    return {};
}

}  // namespace solid_space::corpus
