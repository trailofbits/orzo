#
# Copyright (c) 2023-present, Trail of Bits, Inc. All rights reserved.
#
# This source code is licensed in accordance with the terms specified in the
# LICENSE file found in the root directory of this source tree.
#

cmake_minimum_required(VERSION 3.29)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(macroni/project_version_details)

project(
  macroni
  LANGUAGES C CXX
  VERSION ${macroni_PROJECT_VERSION_INTERNAL}
  DESCRIPTION "Intermediate representation of macros for program analysis"
  HOMEPAGE_URL "https://github.com/trailofbits/macroni.git")

set(BUG_REPORT_URL
    "https://github.com/trailofbits/macroni/issues"
    CACHE STRING "")

include(macroni/prevent_in_source_builds)

# Check if macroni is being used directly or via add_subdirectory, but allow
# overriding
if(NOT DEFINED MACRONI_MASTER_PROJECT)
  set(MACRONI_MASTER_PROJECT
      "$<STREQUAL:${CMAKE_CURRENT_SOURCE_DIR},${CMAKE_SOURCE_DIR}>")
endif()

include(macroni/project_settings)
include(macroni/project_utils)

set(MACRONI_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(MACRONI_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

#
# Find Git
#
find_package(Git REQUIRED)
include(macroni/get_git_hash)
get_git_hash(macroni_GIT_HASH)
include(macroni/count_commits_since_last_version_change)
count_commits_since_last_version_change(
  macroni_COMMITS_SINCE_LAST_VERSION_CHANGE)

#
# Find CCACHE
#
find_program(CCACHE ccache)
if(CCACHE)
  set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE}")
  set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE}")
endif()

#
# Find LLVM
#
find_package(LLVM 18.1.4 REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# These lines tell LLVM where to put any targets we create with calls to
# add_clang_executable() (and presumably other functions like it).
#
# TODO(Brent): See if we can place these binaries where ninja-multi-config would
# place them so we don't overwrite release binaries with debug and vice-versa.
set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/bin)
set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/lib)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(TableGen)
include(AddLLVM)
include(HandleLLVMOptions)
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

include_directories(SYSTEM ${LLVM_INCLUDE_DIRS})
link_directories(${LLVM_BUILD_LIBRARY_DIR})
add_definitions(${LLVM_DEFINITIONS})

if(LLVM_LINK_LLVM_DYLIB)
  set(LLVM_LIBS LLVM)
else()
  llvm_map_components_to_libnames(LLVM_LIBS ${LLVM_TARGETS_TO_BUILD} target
                                  option)
endif()

#
# Find Clang
#
find_package(Clang REQUIRED CONFIG)
find_and_select_clang_compiler()
message(STATUS "Using ClangConfig.cmake in: ${Clang_DIR}")

list(APPEND CMAKE_MODULE_PATH "${Clang_DIR}")
include(AddClang)
add_definitions(${CLANG_DEFINITIONS})

if(CLANG_LINK_CLANG_DYLIB)
  set(CLANG_LIBS clang-cpp)
else()
  set(CLANG_LIBS
      clangAST
      clangASTMatchers
      clangBasic
      clangCodeGen
      clangDriver
      clangFrontend
      clangSerialization
      clangTooling)
endif()

#
# Find MLIR
#
find_package(MLIR REQUIRED CONFIG)
message(STATUS "Using MLIRConfig.cmake in: ${MLIR_DIR}")

list(APPEND CMAKE_MODULE_PATH "${MLIR_CMAKE_DIR}")
include(AddMLIR)

include_directories(SYSTEM ${MLIR_INCLUDE_DIRS})

if(MLIR_LINK_MLIR_DYLIB)
  set(MACRONI_MLIR_LIBS MLIR)
else()
  get_property(MLIR_DIALECT_LIBS GLOBAL PROPERTY MLIR_DIALECT_LIBS)
  get_property(MLIR_CONVERSION_LIBS GLOBAL PROPERTY MLIR_CONVERSION_LIBS)
  get_property(MLIR_EXTENSION_LIBS GLOBAL PROPERTY MLIR_EXTENSION_LIBS)
  get_property(MLIR_TRANSLATION_LIBS GLOBAL PROPERTY MLIR_TRANSLATION_LIBS)

  set(MACRONI_MLIR_LIBS
      MLIRAnalysis
      MLIRDialect
      MLIRExecutionEngine
      MLIRIR
      MLIRParser
      MLIRPass
      MLIRSupport
      MLIRTransforms
      MLIRTransformUtils
      ${MLIR_DIALECT_LIBS}
      ${MLIR_CONVERSION_LIBS}
      ${MLIR_EXTENSION_LIBS}
      ${MLIR_TRANSLATION_LIBS})
endif()

#
# macroni build settings
#

add_custom_target(macroni-headers)
set_target_properties(macroni-headers PROPERTIES FOLDER "Misc")

define_property(GLOBAL PROPERTY MACRONI_INSTALL_TARGETS)

add_library(macroni_settings INTERFACE)

include(macroni/compiler_warnings)
set_target_compiler_warnings(macroni_settings)

target_include_directories(
  macroni_settings
  INTERFACE $<BUILD_INTERFACE:${MACRONI_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${MACRONI_BINARY_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# enable exception on failure
option(MACRONI_ENABLE_EXCEPTIONS "Enable exception throw on macroni failure" ON)
if(MACRONI_ENABLE_EXCEPTIONS)
  target_compile_definitions(macroni_settings
                             INTERFACE -DMACRONI_ENABLE_EXCEPTIONS)

  target_compile_options(macroni_settings INTERFACE -fexceptions)
endif()

# sanitizer options if supported by compiler
include(macroni/sanitizers)
enable_sanitizers(macroni_settings)

# allow for static analysis options
include(macroni/static_analyzers)

option(ENABLE_PDLL_CONVERSIONS "Enable PDLL conversions" OFF)
if(ENABLE_PDLL_CONVERSIONS)
  message(STATUS "ENABLE_PDLL_CONVERSIONS")
  target_compile_definitions(macroni_settings
                             INTERFACE -DENABLE_PDLL_CONVERSIONS)
endif()

add_library(macroni::settings ALIAS macroni_settings)

#
# Add external libraries
#
add_subdirectory(external)

# TODO(bpp): Figure out which exact targets require this definition to compile;
# right now we turn it on project-wide to prevent issues but it would be better
# to enable it only where it is needed.
add_compile_definitions(GAP_ENABLE_COROUTINES)
# TOOD(bpp): Add the option to use a vendored version of gap. This is more
# important now that VAST does not offer a vendored version of gap. Also this
# would prevent the user from accidentally compiling gap with GCC and then
# trying to compile Macroni with Clang, which would cause compilation errors due
# to incompatible GCC warnings being passed to Clang.
find_package(gap CONFIG REQUIRED)

if(NOT MACRONI_USE_VENDORED_PASTA)
  find_package(pasta CONFIG REQUIRED)
endif()

if(NOT MACRONI_USE_VENDORED_VAST)
  find_package(VAST CONFIG REQUIRED)
endif()

# TODO(Brent): Only link vast libs to targets that need them.
get_property(VAST_LIBS GLOBAL PROPERTY VAST_ALL_LIBS)
target_link_libraries(
  macroni_settings INTERFACE ${LLVM_LIBS} ${CLANG_LIBS} ${MACRONI_MLIR_LIBS}
                             gap::gap vast_settings ${VAST_LIBS})

include_directories(${MACRONI_SOURCE_DIR}/include)
include_directories(${MACRONI_BINARY_DIR}/include)

#
# macroni libraries
#
add_subdirectory(include/macroni)
add_subdirectory(lib)

#
# macroni executables
#
add_subdirectory(bin)

#
# macroni tests
#
option(MACRONI_ENABLE_TESTING "Enable Macroni's test suite"
       ${PROJECT_IS_TOP_LEVEL})
if(MACRONI_ENABLE_TESTING)
  enable_testing()
  add_subdirectory(test)
endif()

#
# Install settings
#

option(MACRONI_INSTALL "Generate the install target." ${MACRONI_MASTER_PROJECT})

if(MACRONI_INSTALL)
  set(MACRONI_CMAKE_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

  get_property(MACRONI_INSTALL_TARGETS GLOBAL PROPERTY MACRONI_INSTALL_TARGETS)

  install(
    DIRECTORY ${MACRONI_BINARY_DIR}/include/macroni
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    COMPONENT macroni-headers
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.hpp"
    PATTERN "*.h.inc"
    PATTERN "*.hpp.inc"
    PATTERN "CMakeFiles" EXCLUDE)

  install(
    DIRECTORY ${MACRONI_SOURCE_DIR}/include/macroni
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    COMPONENT macroni-headers
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "*.hpp"
    PATTERN "CMakeFiles" EXCLUDE)

  install(TARGETS macroni_settings EXPORT macroniTargets)

  set(MACRONI_EXPORT_NAME macroniTargets)

  install(
    EXPORT macroniTargets
    FILE ${MACRONI_EXPORT_NAME}.cmake
    NAMESPACE macroni::
    DESTINATION ${MACRONI_CMAKE_INSTALL_DIR})

  #
  # packaging support
  #

  set(CPACK_PACKAGE_VENDOR "Trail of Bits")
  set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
      "macroni: A C/C++ frontend for lowering macros down to MLIR")
  set(CPACK_PACKAGE_VERSION_MAJOR ${CMAKE_PROJECT_VERSION_MAJOR})
  set(CPACK_PACKAGE_VERSION_MINOR ${CMAKE_PROJECT_VERSION_MINOR})
  set(CPACK_PACKAGE_VERSION_PATCH ${CMAKE_PROJECT_VERSION_PATCH})
  set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
  set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
  set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/package")
  set(CPACK_PACKAGE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  set(SYSTEM
      ${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_VERSION})
  set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-${SYSTEM}")
  set(CPACK_SOURCE_PACKAGE_FILE_NAME "${PROJECT_NAME}-${PROJECT_VERSION}-src")

  include(CPack)

  set(MACRONI_CONFIG_NAME "${PROJECT_NAME}Config")
  set(MACRONI_PACKAGE_CONFIG_FILE "${MACRONI_CONFIG_NAME}.cmake")
  set(MACRONI_PACKAGE_CONFIG_VERSION_FILE "${MACRONI_CONFIG_NAME}Version.cmake")

  include(CMakePackageConfigHelpers)

  configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${MACRONI_CONFIG_NAME}.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/${MACRONI_PACKAGE_CONFIG_FILE}"
    INSTALL_DESTINATION ${MACRONI_CMAKE_INSTALL_DIR})

  write_basic_package_version_file(
    ${MACRONI_PACKAGE_CONFIG_VERSION_FILE}
    VERSION ${PACKAGE_VERSION}
    COMPATIBILITY SameMajorVersion)

  install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/${MACRONI_PACKAGE_CONFIG_FILE}"
          "${CMAKE_CURRENT_BINARY_DIR}/${MACRONI_PACKAGE_CONFIG_VERSION_FILE}"
    DESTINATION ${MACRONI_CMAKE_INSTALL_DIR})

endif()
