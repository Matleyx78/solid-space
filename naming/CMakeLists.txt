# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The solid-space authors

# naming depends only on the standard library (ADR 0002).
add_library(solid_space_naming STATIC src/naming.cpp)
add_library(solid_space::naming ALIAS solid_space_naming)

# Public headers will live in include/solid_space/naming/ (ADR 0003, point 4).
target_include_directories(solid_space_naming PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(solid_space_naming PUBLIC cxx_std_20)

ss_configure_target(solid_space_naming)
