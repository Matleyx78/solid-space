// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

namespace solid_space::geom2d {

// Placeholder with external linkage, as in the other components: it keeps the
// static library from being empty (the macOS archiver warns about it) and
// gives the test bench something to link against. The public headers are a
// draft contract with no implementation yet (ADR 0007, point 15); the anchor
// goes away with the first real code of the component.
extern const int kSkeletonAnchor;
const int kSkeletonAnchor = 0;

}  // namespace solid_space::geom2d
