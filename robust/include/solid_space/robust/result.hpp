// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

#pragma once

// Draft contract (ADR 0007, point 15): see ADR 0008, point 7. This vocabulary
// type is complete here, because a class template lives in its header; it
// does no floating-point arithmetic (ADR 0008, point 5).

#include <cassert>
#include <type_traits>
#include <utility>
#include <variant>

namespace solid_space::robust {

/// The outcome of an operation that can fail (ADR 0003, point 5): either a
/// value of type T or a diagnosis of type D, never both. Each component has
/// its own diagnosis type. No exception crosses the interface: reading the
/// side that is not there violates a precondition (an assertion in debug).
template <typename T, typename D>
class Result {
    static_assert(!std::is_same_v<T, D>, "a value and a diagnosis must be told apart");

public:
    // Implicit on purpose: an operation returns either its value or its
    // diagnosis, and both read naturally in a return statement.
    Result(T value) : m_content(std::in_place_index<0>, std::move(value)) {}

    Result(D diagnosis) : m_content(std::in_place_index<1>, std::move(diagnosis)) {}

    /// True when the operation gave a value.
    [[nodiscard]] bool hasValue() const noexcept { return m_content.index() == 0; }

    /// The value. Precondition: hasValue().
    [[nodiscard]] const T& value() const& {
        assert(hasValue());
        return std::get<0>(m_content);
    }

    /// The value, moved out. Precondition: hasValue().
    [[nodiscard]] T&& value() && {
        assert(hasValue());
        return std::get<0>(std::move(m_content));
    }

    /// Why there is no value. Precondition: !hasValue().
    [[nodiscard]] const D& diagnosis() const& {
        assert(!hasValue());
        return std::get<1>(m_content);
    }

private:
    std::variant<T, D> m_content;
};

}  // namespace solid_space::robust
