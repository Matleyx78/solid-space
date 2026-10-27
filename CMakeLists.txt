# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The solid-space authors

cmake_minimum_required(VERSION 3.24)

project(solid_space LANGUAGES CXX)

# Build options (ADR 0006, points 1 and 2).
option(SOLID_SPACE_BUILD_KERNEL
       "Build the kernel and features components, the only ones that need OCCT" ON)
option(SOLID_SPACE_WARNINGS_AS_ERRORS
       "Treat compiler warnings as errors (the project presets turn it on)" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Included by path, not through CMAKE_MODULE_PATH, so that a project embedding
# this one does not see its module directory.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/SolidSpaceWarnings.cmake")

# One target per component, in dependency order (ADR 0002): a component only
# links the ones defined before it, so the graph cannot have cycles.
add_subdirectory(robust)
add_subdirectory(naming)
add_subdirectory(solver)
if(SOLID_SPACE_BUILD_KERNEL)
  add_subdirectory(kernel)
  add_subdirectory(features)
endif()

# Tests are built only when this is the top-level project, so that a project
# embedding the library does not build them.
if(PROJECT_IS_TOP_LEVEL)
  include(CTest)
  if(BUILD_TESTING)
    add_subdirectory(tests)
  endif()
endif()
