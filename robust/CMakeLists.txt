# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The solid-space authors

# robust depends only on the standard library (ADR 0002).
add_library(solid_space_robust STATIC src/robust.cpp)
add_library(solid_space::robust ALIAS solid_space_robust)

# Public headers will live in include/solid_space/robust/ (ADR 0003, point 4).
target_include_directories(solid_space_robust PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(solid_space_robust PUBLIC cxx_std_20)

ss_configure_target(solid_space_robust)
