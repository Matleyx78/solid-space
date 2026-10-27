# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The solid-space authors

# features depends on kernel, naming and solver (ADR 0002). The root
# CMakeLists.txt defines this target only when SOLID_SPACE_BUILD_KERNEL is on,
# because it needs kernel.
add_library(solid_space_features STATIC src/features.cpp)
add_library(solid_space::features ALIAS solid_space_features)

# Public headers will live in include/solid_space/features/ (ADR 0003, point 4).
target_include_directories(solid_space_features PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(solid_space_features PUBLIC cxx_std_20)
target_link_libraries(solid_space_features PUBLIC
  solid_space::kernel solid_space::naming solid_space::solver)

ss_configure_target(solid_space_features)
