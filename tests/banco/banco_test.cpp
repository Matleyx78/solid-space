// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The solid-space authors

// Test bench for the build skeleton (stage 0.1, package P2). It has no
// geometry: it proves that the test framework runs, that every component target
// links through its alias, and that the kernel components follow the build
// option. The CMake file passes the SS_BANCO_* values.

#include <gtest/gtest.h>

// The skeleton anchors are private to each component; they are declared here
// only to prove that the component library links. They are removed together
// with the anchors.
namespace solid_space::robust {
extern const int kSkeletonAnchor;
}
namespace solid_space::naming {
extern const int kSkeletonAnchor;
}
namespace solid_space::solver {
extern const int kSkeletonAnchor;
}
#if SS_BANCO_HAS_KERNEL
namespace solid_space::kernel {
extern const int kSkeletonAnchor;
}
#endif
#if SS_BANCO_HAS_FEATURES
namespace solid_space::features {
extern const int kSkeletonAnchor;
}
#endif

TEST(Banco, ComponentsAlwaysBuiltLink) {
  EXPECT_EQ(solid_space::robust::kSkeletonAnchor, 0);
  EXPECT_EQ(solid_space::naming::kSkeletonAnchor, 0);
  EXPECT_EQ(solid_space::solver::kSkeletonAnchor, 0);
}

#if SS_BANCO_HAS_KERNEL
TEST(Banco, KernelLinksWhenPresent) {
  EXPECT_EQ(solid_space::kernel::kSkeletonAnchor, 0);
}
#endif

#if SS_BANCO_HAS_FEATURES
TEST(Banco, FeaturesLinksWhenPresent) {
  EXPECT_EQ(solid_space::features::kSkeletonAnchor, 0);
}
#endif

// With SOLID_SPACE_BUILD_KERNEL off, kernel and features must not exist (and
// so OCCT is not needed); with it on, both must exist.
TEST(Banco, KernelComponentsFollowTheBuildOption) {
  EXPECT_EQ(SS_BANCO_HAS_KERNEL, SS_BANCO_OPTION_BUILD_KERNEL);
  EXPECT_EQ(SS_BANCO_HAS_FEATURES, SS_BANCO_OPTION_BUILD_KERNEL);
}
