cmake_minimum_required(VERSION 3.28)
project(PirateHatHUD VERSION 0.2.0 LANGUAGES CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "PirateHatHUD requires Windows x64")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
include(FetchContent)
set(SAFETYHOOK_FETCH_ZYDIS ON CACHE BOOL "Fetch Zydis" FORCE)
FetchContent_Declare(safetyhook GIT_REPOSITORY https://github.com/cursey/safetyhook.git GIT_TAG origin/main)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.92.9b)
FetchContent_MakeAvailable(safetyhook imgui)

add_library(imgui_static STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp)
target_include_directories(imgui_static PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_compile_definitions(imgui_static PRIVATE IMGUI_DISABLE_OBSOLETE_FUNCTIONS)

add_library(PirateHatHUD SHARED src/main.cpp src/pattern_scan.cpp)
target_include_directories(PirateHatHUD PRIVATE include)
target_compile_definitions(PirateHatHUD PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_compile_options(PirateHatHUD PRIVATE /W4 /EHsc)
target_link_libraries(PirateHatHUD PRIVATE safetyhook imgui_static d3d11 dxgi)
set_target_properties(PirateHatHUD PROPERTIES PREFIX "" SUFFIX ".asi")

add_custom_command(TARGET PirateHatHUD POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:PirateHatHUD>" "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD/PirateHatHUD.asi"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/config.ini" "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD/config.ini"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md" "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD/THIRD_PARTY_NOTICES.md"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${safetyhook_SOURCE_DIR}/LICENSE" "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD/LICENSE-SafetyHook.txt"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${imgui_SOURCE_DIR}/LICENSE.txt" "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD/LICENSE-DearImGui.txt")

include(CTest)
if(BUILD_TESTING)
  add_executable(scanner_tests tests/scanner_tests.cpp src/pattern_scan.cpp)
  target_include_directories(scanner_tests PRIVATE include)
  target_compile_definitions(scanner_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  add_test(NAME scanner_tests COMMAND scanner_tests)
endif()
