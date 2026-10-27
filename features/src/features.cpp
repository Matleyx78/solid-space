// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

namespace solid_space::features {

// Placeholder with external linkage. It keeps the static library from being
// empty, which makes the macOS archiver warn ("no symbols"), and it gives the
// test bench something to link against. It goes away with the first real code
// of the component.
extern const int kSkeletonAnchor;
const int kSkeletonAnchor = 0;

}  // namespace solid_space::features
