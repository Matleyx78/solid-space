# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 The solid-space authors

# Verify the actual target options, independently of the helper's invocation.
foreach(component robust geom2d)
  get_target_property(ss_options solid_space_${component} COMPILE_OPTIONS)
  if(MSVC)
    if(NOT "/fp:precise" IN_LIST ss_options OR "/fp:contract" IN_LIST ss_options)
      message(FATAL_ERROR "ADR 0008: ${component} must use /fp:precise without /fp:contract")
    endif()
  elseif(NOT "-ffp-contract=off" IN_LIST ss_options)
    message(FATAL_ERROR "ADR 0008: ${component} must use -ffp-contract=off")
  endif()
endforeach()

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
  solid_space::robust solid_space::geom2d solid_space::naming solid_space::solver
  GTest::gtest_main)

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

# Draft contracts (ADR 0007, point 15): the public headers of robust and
# geom2d compile and keep their declared signatures. They call nothing that is
# not implemented yet.
add_executable(solid_space_contract_test robust/contract_test.cpp geom2d/contract_test.cpp)
target_link_libraries(solid_space_contract_test PRIVATE
  solid_space::robust solid_space::geom2d GTest::gtest_main)
ss_configure_target(solid_space_contract_test)
gtest_discover_tests(solid_space_contract_test DISCOVERY_MODE PRE_TEST)

# Globs keep parallel stage packages from editing shared CMake files.
foreach(component robust geom2d)
  file(GLOB ss_unit_sources CONFIGURE_DEPENDS ${component}/*_test.cpp)
  list(FILTER ss_unit_sources EXCLUDE REGEX "/contract_test\\.cpp$")
  if(ss_unit_sources)
    set(ss_unit_target solid_space_${component}_unit_test)
    add_executable(${ss_unit_target} ${ss_unit_sources})
    target_include_directories(${ss_unit_target} PRIVATE
      ${PROJECT_SOURCE_DIR}/robust/src ${PROJECT_SOURCE_DIR}/geom2d/src)
    target_link_libraries(${ss_unit_target} PRIVATE
      solid_space::${component} GTest::gtest_main)
    ss_configure_target(${ss_unit_target})
    gtest_discover_tests(${ss_unit_target} DISCOVERY_MODE PRE_TEST)
  endif()
endforeach()

# Verification corpus (ADR 0005): the structural validator and its own tests,
# under ctest with the label "corpus". Python is a development tool, not a
# dependency of the library, so it is needed here and nowhere else. -B keeps
# bytecode files out of the source tree.
find_package(Python3 3.9 REQUIRED COMPONENTS Interpreter)
add_test(NAME corpus_validate
  COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/corpus/valida_corpus.py
          ${CMAKE_CURRENT_SOURCE_DIR}/corpus)
add_test(NAME corpus_validator_tests
  COMMAND ${Python3_EXECUTABLE} -B ${CMAKE_CURRENT_SOURCE_DIR}/corpus/test_valida_corpus.py)
set_tests_properties(corpus_validate corpus_validator_tests PROPERTIES LABELS corpus)

# Compile the corpus into an immutable table, including pending cases. Watch
# directories as well as files so adding/removing a case reruns generation.
set(ss_corpus_dir ${CMAKE_CURRENT_SOURCE_DIR}/corpus)
set(ss_corpus_output ${CMAKE_BINARY_DIR}/corpus)
file(GLOB_RECURSE ss_corpus_cases CONFIGURE_DEPENDS ${ss_corpus_dir}/casi/*.json)
add_custom_command(
  OUTPUT ${ss_corpus_output}/casi.cpp ${ss_corpus_output}/obbligatori.txt
  COMMAND ${Python3_EXECUTABLE} -B ${ss_corpus_dir}/genera_casi.py
          ${ss_corpus_dir} ${ss_corpus_output} --componenti robust geom2d
  DEPENDS ${ss_corpus_cases} ${ss_corpus_dir}/in-attesa.txt
          ${ss_corpus_dir}/sorgenti.json ${ss_corpus_dir}/genera_casi.py
          ${ss_corpus_dir}/valida_corpus.py
  VERBATIM)
file(GLOB ss_corpus_operations CONFIGURE_DEPENDS ${ss_corpus_dir}/esegui/op_*.cpp)
set(ss_corpus_support
  corpus/esegui/value.cpp corpus/esegui/operations.cpp corpus/esegui/plane_data.cpp)
add_executable(solid_space_corpus_test corpus/esegui/main.cpp
  ${ss_corpus_support} ${ss_corpus_operations} ${ss_corpus_output}/casi.cpp)
target_include_directories(solid_space_corpus_test PRIVATE ${ss_corpus_dir}/esegui)
target_link_libraries(solid_space_corpus_test PRIVATE
  solid_space::robust solid_space::geom2d GTest::gtest)
ss_configure_target(solid_space_corpus_test)
gtest_discover_tests(solid_space_corpus_test DISCOVERY_MODE PRE_TEST PROPERTIES LABELS corpus)
add_test(NAME corpus_results COMMAND solid_space_corpus_test
  --ss-results=${ss_corpus_output}/risultati.txt)
set_tests_properties(corpus_results PROPERTIES LABELS "corpus;determinismo")

# A separate process/table exercises the runner without polluting the archive
# or registering fake operations in the real corpus executable.
add_executable(solid_space_corpus_value_test corpus/esegui/value_test.cpp
  corpus/esegui/main.cpp ${ss_corpus_support})
target_compile_definitions(solid_space_corpus_value_test PRIVATE SS_CORPUS_TESTING)
target_link_libraries(solid_space_corpus_value_test PRIVATE
  solid_space::robust solid_space::geom2d GTest::gtest_main)
ss_configure_target(solid_space_corpus_value_test)
gtest_discover_tests(solid_space_corpus_value_test DISCOVERY_MODE PRE_TEST PROPERTIES LABELS corpus)
add_test(NAME corpus_generator_tests
  COMMAND ${Python3_EXECUTABLE} -B ${ss_corpus_dir}/test_genera_casi.py
          $<TARGET_FILE:solid_space_corpus_test>)
set_tests_properties(corpus_generator_tests PROPERTIES LABELS corpus)

add_test(NAME determinism_comparison_tests
  COMMAND ${Python3_EXECUTABLE} -B ${PROJECT_SOURCE_DIR}/scripts/test_confronta_risultati.py)
set_tests_properties(determinism_comparison_tests PROPERTIES LABELS determinismo)
