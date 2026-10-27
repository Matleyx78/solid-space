# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The solid-space authors

# kernel depends on robust and naming (ADR 0002). OCCT is linked privately from
# stage 0.5 on; it is not linked yet. The root CMakeLists.txt defines this
# target only when SOLID_SPACE_BUILD_KERNEL is on.
add_library(solid_space_kernel STATIC src/kernel.cpp)
add_library(solid_space::kernel ALIAS solid_space_kernel)

# Public headers will live in include/solid_space/kernel/ (ADR 0003, point 4).
target_include_directories(solid_space_kernel PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(solid_space_kernel PUBLIC cxx_std_20)
target_link_libraries(solid_space_kernel PUBLIC solid_space::robust solid_space::naming)

ss_configure_target(solid_space_kernel)
