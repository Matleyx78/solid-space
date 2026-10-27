# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The solid-space authors

# GoogleTest: the system package if there is one, otherwise a pinned download
# verified by its hash (ADR 0006, point 4). Only the download needs the network.
find_package(GTest CONFIG QUIET)
if(NOT GTest_FOUND)
  include(FetchContent)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_Declare(googletest
    URL https://github.com/google/googletest/releases/download/v1.17.0/googletest-1.17.0.tar.gz
    URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(googletest)
endif()
include(GoogleTest)

# Test bench: links the alias of every component that exists and checks that
# the kernel components follow SOLID_SPACE_BUILD_KERNEL.
add_executable(solid_space_banco_test banco/banco_test.cpp)
target_link_libraries(solid_space_banco_test PRIVATE
  solid_space::robust solid_space::naming solid_space::solver GTest::gtest_main)

set(ss_banco_has_kernel 0)
if(TARGET solid_space::kernel)
  set(ss_banco_has_kernel 1)
  target_link_libraries(solid_space_banco_test PRIVATE solid_space::kernel)
endif()
set(ss_banco_has_features 0)
if(TARGET solid_space::features)
  set(ss_banco_has_features 1)
  target_link_libraries(solid_space_banco_test PRIVATE solid_space::features)
endif()

target_compile_definitions(solid_space_banco_test PRIVATE
  SS_BANCO_OPTION_BUILD_KERNEL=$<BOOL:${SOLID_SPACE_BUILD_KERNEL}>
  SS_BANCO_HAS_KERNEL=${ss_banco_has_kernel}
  SS_BANCO_HAS_FEATURES=${ss_banco_has_features})

ss_configure_target(solid_space_banco_test)
gtest_discover_tests(solid_space_banco_test DISCOVERY_MODE PRE_TEST)
