# Can run independently of the Windows-only build and its dependencies.
cmake_minimum_required(VERSION 3.28)
get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

if(NOT CLANG_FORMAT)
  unset(CLANG_FORMAT)
  unset(CLANG_FORMAT CACHE)
  find_program(CLANG_FORMAT NAMES clang-format
    HINTS "${PROJECT_ROOT}/build/format-tools/clang_format/data/bin"
    REQUIRED)
endif()

execute_process(COMMAND "${CLANG_FORMAT}" --version
  OUTPUT_VARIABLE FORMAT_VERSION
  RESULT_VARIABLE FORMAT_RESULT)
if(NOT FORMAT_RESULT EQUAL 0 OR NOT FORMAT_VERSION MATCHES "version 20\\.1\\.8([^0-9.]|$)")
  message(FATAL_ERROR "clang-format 20.1.8 is required; got: ${FORMAT_VERSION}")
endif()

file(GLOB_RECURSE FORMAT_SOURCES
  "${PROJECT_ROOT}/src/*.cpp" "${PROJECT_ROOT}/src/*.hpp" "${PROJECT_ROOT}/src/*.h"
  "${PROJECT_ROOT}/include/*.hpp" "${PROJECT_ROOT}/include/*.h"
  "${PROJECT_ROOT}/tests/*.cpp" "${PROJECT_ROOT}/tests/*.hpp" "${PROJECT_ROOT}/tests/*.h")
if(CHECK)
  set(FORMAT_ARGS --dry-run --Werror)
else()
  set(FORMAT_ARGS -i)
endif()
execute_process(
  COMMAND "${CLANG_FORMAT}" --style=file --fallback-style=none ${FORMAT_ARGS} ${FORMAT_SOURCES}
  WORKING_DIRECTORY "${PROJECT_ROOT}"
  RESULT_VARIABLE FORMAT_RESULT)
if(NOT FORMAT_RESULT EQUAL 0)
  message(FATAL_ERROR "C++ formatting failed. Run the format target to fix formatting.")
endif()
