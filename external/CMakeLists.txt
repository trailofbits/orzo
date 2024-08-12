#
# Copyright (c) 2023-present, Trail of Bits, Inc. All rights reserved.
#
# This source code is licensed in accordance with the terms specified in the
# LICENSE file found in the root directory of this source tree.
#

option(MACRONI_USE_VENDORED_PASTA "Enable build with pasta submodule" ON)
option(MACRONI_USE_VENDORED_VAST "Enable build with VAST submodule" ON)

if(MACRONI_USE_VENDORED_PASTA)
  set(PASTA_BOOTSTRAP_MACROS
      OFF
      CACHE BOOL "Bootstrap Pasta's macros? Not necessary")
  set(PASTA_BOOTSTRAP_TYPES
      OFF
      CACHE BOOL "Bootstrap Pasta's types? Not necessary")
  set(PASTA_ENABLE_TESTING
      OFF
      CACHE BOOL "Enable Pasta testing? Not necessary")
  set(PASTA_ENABLE_INSTALL
      OFF
      CACHE BOOL "Enable Pasta install? Not necessary")
  set(PASTA_ENABLE_PY_BINDINGS
      OFF
      CACHE BOOL "Enable Pasta Py bindings? Not necessary")
  set(PASTA_USE_VENDORED_CLANG
      OFF
      CACHE
        BOOL
        "Use Pasta's vendored version of Clang. Will take a long time to build")
  add_subdirectory(pasta)
endif()

if(MACRONI_USE_VENDORED_VAST)
  set(VAST_WARNINGS_AS_ERRORS
      OFF
      CACHE BOOL
            "Tell VAST to treat warnings as errors. Will likely break things")
  set(VAST_BUILD_TOOLS
      OFF
      CACHE BOOL "Build VAST's tools? Not necessary")
  set(VAST_ENABLE_TESTING
      OFF
      CACHE BOOL "Enable VAST testing? Not necessary")
  add_subdirectory(vast)
endif()
