# Only execute this file once for the whole project, in the same namespace it is
# included in.

include_guard(GLOBAL)

# Append to message context here and pop changes at the end of this file.
list(APPEND CMAKE_MESSAGE_CONTEXT "projectSettings")

# If macroni is the root project and we are using a single-configuration
# generator, but no configuration was specified, switch to a default
# configuration.
set(MACRONI_DEFAULT_CONFIG Debug)
if(MACRONI_MASTER_PROJECT
   AND NOT CMAKE_BUILD_TYPE
   AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "No configuration specified.")
  message(STATUS "Defaulting to '${MACRONI_DEFAULT_CONFIG}'")
  set(CMAKE_BUILD_TYPE
      ${MACRONI_DEFAULT_CONFIG}
      CACHE STRING "Choose the type of build." FORCE)

  # Set the possible values of build type for cmake-gui and ccmake
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release"
                                               "MinSizeRel" "RelWithDebInfo")
endif()

message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")

# Generate a compile commands JSON file for LSP support (e.g., clangd).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Toggle interprocedural optimization
option(ENABLE_IPO
       "Enable Interprocedural Optimization, aka Link Time Optimization (LTO)"
       OFF)

if(ENABLE_IPO)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT result OUTPUT output)
  if(result)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
  else()
    message(WARNING "IPO is not supported: ${output}")
  endif()
endif()

if(UNIX)
  if(APPLE)
    set(PLATFORM_NAME "macos")
  else()
    set(PLATFORM_NAME "linux")
  endif()

elseif(WIN32)
  set(PLATFORM_NAME "windows")

else()
  message("This platform is not officially supported")
endif()

# Pop message log context changes.
list(POP_BACK CMAKE_MESSAGE_CONTEXT)
