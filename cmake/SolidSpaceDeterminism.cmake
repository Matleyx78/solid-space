# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The solid-space authors

# Keep floating-point expressions identical across the ADR 0008 platforms.
function(ss_configure_determinism target)
  if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "ADR 0008: deterministic geometry requires a 64-bit platform")
  endif()

  get_target_property(ss_options ${target} COMPILE_OPTIONS)
  get_directory_property(ss_directory_options COMPILE_OPTIONS)
  set(ss_flags "${ss_options};${ss_directory_options};${CMAKE_CXX_FLAGS}")
  # Also reject configuration-specific flags, including custom configurations.
  get_cmake_property(ss_variables VARIABLES)
  foreach(ss_variable IN LISTS ss_variables)
    if(ss_variable MATCHES "^CMAKE_CXX_FLAGS_")
      string(APPEND ss_flags ";${${ss_variable}}")
    endif()
  endforeach()
  if(ss_flags MATCHES "-ffast-math|-Ofast|-funsafe-math-optimizations|/fp:fast|/fp:contract")
    message(FATAL_ERROR "ADR 0008: ${target} has forbidden floating-point options: ${ss_flags}")
  endif()

  if(MSVC)
    target_compile_options(${target} PRIVATE /fp:precise)
  elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -ffp-contract=off)
  else()
    message(FATAL_ERROR "ADR 0008: unsupported deterministic compiler ${CMAKE_CXX_COMPILER_ID}")
  endif()
endfunction()
