# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The solid-space authors

# geom2d depends only on the standard library and robust (ADR 0008). robust is
# public because the geom2d headers use its types.
# Globs keep parallel stage packages from editing shared CMake files.
file(GLOB ss_geom2d_sources CONFIGURE_DEPENDS src/*.cpp src/exact/*.cpp)
add_library(solid_space_geom2d STATIC ${ss_geom2d_sources})
add_library(solid_space::geom2d ALIAS solid_space_geom2d)

# Public headers live in include/solid_space/geom2d/ (ADR 0003, point 4).
target_include_directories(solid_space_geom2d PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(solid_space_geom2d PUBLIC cxx_std_20)
target_link_libraries(solid_space_geom2d PUBLIC solid_space::robust)

ss_configure_target(solid_space_geom2d)
ss_configure_determinism(solid_space_geom2d)
