# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The solid-space authors

# solver depends on robust (ADR 0002). libslvs is linked privately from stage
# 0.3 on; it is not linked yet.
add_library(solid_space_solver STATIC src/solver.cpp)
add_library(solid_space::solver ALIAS solid_space_solver)

# Public headers will live in include/solid_space/solver/ (ADR 0003, point 4).
target_include_directories(solid_space_solver PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(solid_space_solver PUBLIC cxx_std_20)
target_link_libraries(solid_space_solver PUBLIC solid_space::robust)

ss_configure_target(solid_space_solver)
