cmake_minimum_required(VERSION 3.28)
project(PirateHatHUD VERSION 0.3.0 LANGUAGES CXX)

set(CLANG_FORMAT "" CACHE FILEPATH "Path to clang-format 20.1.8")
add_custom_target(format
  COMMAND ${CMAKE_COMMAND} "-DCLANG_FORMAT=${CLANG_FORMAT}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Format.cmake"
  VERBATIM)
add_custom_target(format-check
  COMMAND ${CMAKE_COMMAND} "-DCLANG_FORMAT=${CLANG_FORMAT}" -DCHECK=ON
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Format.cmake"
  VERBATIM)

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
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp)
target_include_directories(imgui_static PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_compile_definitions(imgui_static PRIVATE IMGUI_DISABLE_OBSOLETE_FUNCTIONS)

add_library(game_observers STATIC src/game/treasure_observer.cpp src/pattern_scan.cpp)
target_include_directories(game_observers PUBLIC include)
target_compile_definitions(game_observers PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_compile_options(game_observers PRIVATE /W4 /EHsc)
target_link_libraries(game_observers PRIVATE safetyhook)

add_library(treasure_indicator STATIC src/features/treasure_indicator.cpp)
target_include_directories(treasure_indicator PUBLIC include)
target_compile_options(treasure_indicator PRIVATE /W4 /EHsc)

add_library(PirateHatHUD SHARED src/main.cpp src/app.cpp src/overlay.cpp
  src/render/dxgi_hooks.cpp src/render/dx12_renderer.cpp src/render/dx12_frames.cpp
  src/render/dx12_texture.cpp src/render/image.cpp src/render/hud_draw.cpp
  src/platform/config.cpp src/platform/hotkeys.cpp src/platform/logger.cpp)
target_include_directories(PirateHatHUD PRIVATE include)
target_compile_definitions(PirateHatHUD PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_compile_options(PirateHatHUD PRIVATE /W4 /EHsc)
target_link_libraries(PirateHatHUD PRIVATE game_observers treasure_indicator safetyhook
  imgui_static d3d12 dxgi ole32 windowscodecs)
set_target_properties(PirateHatHUD PROPERTIES PREFIX "" SUFFIX ".asi")

add_custom_command(TARGET PirateHatHUD POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:PirateHatHUD>" "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD/PirateHatHUD.asi"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/config.ini" "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD/config.ini"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/assets/icon.png" "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD/icon.png"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md" "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD/THIRD_PARTY_NOTICES.md"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${safetyhook_SOURCE_DIR}/LICENSE" "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD/LICENSE-SafetyHook.txt"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${imgui_SOURCE_DIR}/LICENSE.txt" "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD/LICENSE-DearImGui.txt"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_BINARY_DIR}/_deps/zydis-src/LICENSE" "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD/LICENSE-Zydis.txt")

include(CTest)
if(BUILD_TESTING)
  add_executable(scanner_tests tests/scanner_tests.cpp src/pattern_scan.cpp)
  target_include_directories(scanner_tests PRIVATE include)
  target_compile_definitions(scanner_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  add_test(NAME scanner_tests COMMAND scanner_tests)
  add_executable(events_tests tests/events_tests.cpp)
  target_include_directories(events_tests PRIVATE include)
  add_test(NAME events_tests COMMAND events_tests)
  add_executable(indicator_tests tests/indicator_tests.cpp)
  target_link_libraries(indicator_tests PRIVATE treasure_indicator)
  add_test(NAME indicator_tests COMMAND indicator_tests)
  add_executable(observer_tests tests/observer_tests.cpp)
  target_link_libraries(observer_tests PRIVATE game_observers)
  target_compile_definitions(observer_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  add_test(NAME observer_tests COMMAND observer_tests)
  add_executable(image_tests tests/image_tests.cpp src/render/image.cpp)
  target_include_directories(image_tests PRIVATE include)
  target_compile_definitions(image_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(image_tests PRIVATE ole32 windowscodecs)
  add_test(NAME image_tests COMMAND image_tests "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png")
endif()
