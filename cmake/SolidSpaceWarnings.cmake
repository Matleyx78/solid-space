# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The solid-space authors

# ss_configure_target(<target>)
#
# Applies the project warning set to <target>. Every target of the project
# calls it (ADR 0006, point 2). With SOLID_SPACE_WARNINGS_AS_ERRORS turned on,
# warnings fail the build; the option is off by default so that a project
# embedding the library with another compiler is not broken by a new warning.
function(ss_configure_target target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive-)
    if(SOLID_SPACE_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wshadow
      -Wconversion
      -Wsign-conversion
      -Wold-style-cast
      -Wnon-virtual-dtor
      -Woverloaded-virtual
      -Wcast-align
      -Wnull-dereference
      -Wdouble-promotion
      -Wformat=2)
    if(SOLID_SPACE_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()
