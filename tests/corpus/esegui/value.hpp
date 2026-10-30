// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace solid_space::corpus {

/// Test values deliberately keep exact text separate from floating-point data.
class Value {
public:
    enum class Kind : std::uint8_t {
        boolean,
        number,
        exact,
        approximate,
        name,
        text,
        list,
        group
    };
    using List = std::vector<Value>;
    using Group = std::vector<std::pair<std::string, Value>>;

    static Value boolean(bool value);
    static Value number(double value);
    static Value exact(std::string value, std::optional<double> representable = std::nullopt);
    static Value integer(std::int64_t value);
    static Value approximate(double value, double tolerance);
    static Value name(std::string value);
    static Value text(std::string value);
    static Value list(List value);
    static Value group(Group value);

    [[nodiscard]] Kind kind() const;
    // Accessors require the corresponding kind (checked by assertions).
    [[nodiscard]] bool asBoolean() const;
    [[nodiscard]] double asNumber() const;
    [[nodiscard]] double tolerance() const;
    [[nodiscard]] std::optional<double> representable() const;
    [[nodiscard]] const std::string& asString() const;
    [[nodiscard]] const List& asList() const;
    [[nodiscard]] const Group& asGroup() const;
    // Requires group; nullptr means the key is missing.
    [[nodiscard]] const Value* find(std::string_view key) const;

private:
    explicit Value(Kind kind);
    Kind m_kind;
    bool m_boolean = false;
    double m_number = 0.0;
    double m_tolerance = 0.0;
    std::optional<double> m_representable;
    std::string m_string;
    List m_list;
    Group m_group;
};

[[nodiscard]] std::vector<std::string> compare(const Value& expected, const Value& actual);
[[nodiscard]] std::string canonicalText(const Value& actual);

}  // namespace solid_space::corpus
