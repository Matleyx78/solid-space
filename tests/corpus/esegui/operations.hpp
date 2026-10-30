// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

#include "value.hpp"

#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace solid_space::corpus {

using Operation = Value (*)(const Value& input);

class Registration {
public:
    Registration(std::string_view name, Operation operation);
};

[[nodiscard]] Operation findOperation(std::string_view name);
[[nodiscard]] const std::vector<std::string>& registrationErrors();

struct Case {
    std::string id;
    std::string component;
    int pendingIssue;
    Value input;
    Value expected;
};

// Defined by the generated casi.cpp. Input reading errors are runtime_error
// exceptions inside this test harness; bad_alloc is never intercepted.
[[nodiscard]] std::span<const Case> cases();
[[nodiscard]] Value execute(const Case& testCase);
[[nodiscard]] int writeResults(std::span<const Case> table, const std::filesystem::path& file);
[[nodiscard]] int runResults(std::span<const Case> table, std::span<const std::string_view> arguments);

}  // namespace solid_space::corpus
