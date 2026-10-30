// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#include "operations.hpp"

#include <map>
#include <stdexcept>

namespace solid_space::corpus {
namespace {

struct Registry {
    std::map<std::string, Operation, std::less<>> operations;
    std::vector<std::string> errors;
};

Registry& registry() {
    static Registry instance;
    return instance;
}

}  // namespace

Registration::Registration(std::string_view name, Operation operation) {
    auto& state = registry();
    if (operation == nullptr || !state.operations.emplace(name, operation).second) {
        state.errors.push_back("Invalid or duplicate operation: " + std::string(name));
    }
}

Operation findOperation(std::string_view name) {
    const auto& operations = registry().operations;
    const auto found = operations.find(name);
    return found == operations.end() ? nullptr : found->second;
}

const std::vector<std::string>& registrationErrors() {
    return registry().errors;
}

Value execute(const Case& testCase) {
    if (!registrationErrors().empty()) {
        throw std::runtime_error(registrationErrors().front());
    }
    if (testCase.input.kind() != Value::Kind::group) {
        throw std::runtime_error("ingresso: expected a group");
    }
    const auto* name = testCase.input.find("operazione");
    if (name == nullptr || name->kind() != Value::Kind::text) {
        throw std::runtime_error("operazione: expected operation text");
    }
    const auto operation = findOperation(name->asString());
    if (operation == nullptr) {
        throw std::runtime_error("operazione: unregistered operation " + name->asString());
    }
    return operation(testCase.input);
}

}  // namespace solid_space::corpus
