cmake_minimum_required(VERSION 3.28)
project(PirateHatHUD VERSION 0.4.0 LANGUAGES CXX RC)

include(cmake/Architecture.cmake)
add_custom_target(architecture-check ALL
  COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Architecture.cmake"
  VERBATIM)

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
set(PHI_ICON_RESOURCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png")
configure_file(assets/icon.rc.in "${CMAKE_CURRENT_BINARY_DIR}/generated/icon.rc" @ONLY)
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/generated/icon.rc"
  PROPERTIES OBJECT_DEPENDS "${PHI_ICON_RESOURCE_PATH}")
configure_file(cmake/modinfo.json.in "${CMAKE_CURRENT_BINARY_DIR}/generated/modinfo.json" @ONLY)
include(FetchContent)
set(SAFETYHOOK_FETCH_ZYDIS ON CACHE BOOL "Fetch Zydis" FORCE)
FetchContent_Declare(safetyhook GIT_REPOSITORY https://github.com/cursey/safetyhook.git
  GIT_TAG f44cc070a8340f2f26649553c49533475417304d)
FetchContent_Declare(imgui GIT_REPOSITORY https://github.com/ocornut/imgui.git GIT_TAG v1.92.9b)
FetchContent_MakeAvailable(safetyhook imgui)
get_target_property(PHI_ZYDIS_SOURCE_DIR Zydis SOURCE_DIR)

add_library(imgui_static STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx12.cpp)
target_include_directories(imgui_static PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_compile_definitions(imgui_static PRIVATE IMGUI_DISABLE_OBSOLETE_FUNCTIONS)

add_library(game_observers STATIC src/game/treasure_observer.cpp src/game/minimap_observer.cpp
  src/game/menu_observer.cpp src/game/memory_reader.cpp src/pattern_scan.cpp)
target_include_directories(game_observers PUBLIC include)
target_compile_definitions(game_observers PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_compile_options(game_observers PRIVATE /W4 /EHsc)
target_link_libraries(game_observers PRIVATE safetyhook)

add_library(treasure_indicator STATIC src/features/treasure_indicator.cpp)
target_include_directories(treasure_indicator PUBLIC include)
target_compile_options(treasure_indicator PRIVATE /W4 /EHsc)

add_library(PirateHatHUD SHARED "${CMAKE_CURRENT_BINARY_DIR}/generated/icon.rc"
  src/main.cpp src/app.cpp src/overlay.cpp
  src/render/dxgi_hooks.cpp src/render/dx12_renderer.cpp src/render/dx12_frames.cpp
  src/render/dx12_texture.cpp src/render/dx12_hdr.cpp src/render/hdr_bytecode.cpp
  src/render/image.cpp src/render/hud_draw.cpp
  src/platform/config.cpp src/platform/hotkeys.cpp src/platform/logger.cpp)
target_include_directories(PirateHatHUD PRIVATE include)
target_compile_definitions(PirateHatHUD PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE
  PHI_VERSION="${PROJECT_VERSION}")
target_compile_options(PirateHatHUD PRIVATE /W4 /EHsc)
target_link_libraries(PirateHatHUD PRIVATE game_observers treasure_indicator safetyhook
  imgui_static d3d12 dxgi d3dcompiler ole32 windowscodecs)
set_target_properties(PirateHatHUD PROPERTIES PREFIX "" SUFFIX ".asi")

add_custom_command(TARGET PirateHatHUD POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:PirateHatHUD>" "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD/PirateHatHUD.asi"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/config.ini" "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD/PirateHatHUD.ini"
  COMMAND ${CMAKE_COMMAND} -E rm -f "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD/config.ini" "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD/icon.png"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/generated/modinfo.json" "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD/modinfo.json"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/LICENSE" "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD/LICENSE"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md" "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD/THIRD_PARTY_NOTICES.md"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${safetyhook_SOURCE_DIR}/LICENSE" "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD/LICENSE-SafetyHook.txt"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${imgui_SOURCE_DIR}/LICENSE.txt" "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD/LICENSE-DearImGui.txt"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PHI_ZYDIS_SOURCE_DIR}/LICENSE" "$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD/LICENSE-Zydis.txt")

install(TARGETS PirateHatHUD RUNTIME DESTINATION PirateHatHUD COMPONENT Mod
  LIBRARY DESTINATION PirateHatHUD COMPONENT Mod)
install(FILES config.ini DESTINATION PirateHatHUD RENAME PirateHatHUD.ini COMPONENT Mod)
install(FILES LICENSE THIRD_PARTY_NOTICES.md "${CMAKE_CURRENT_BINARY_DIR}/generated/modinfo.json"
  DESTINATION PirateHatHUD COMPONENT Mod)
install(FILES "${safetyhook_SOURCE_DIR}/LICENSE" DESTINATION PirateHatHUD
  RENAME LICENSE-SafetyHook.txt COMPONENT Mod)
install(FILES "${imgui_SOURCE_DIR}/LICENSE.txt" DESTINATION PirateHatHUD
  RENAME LICENSE-DearImGui.txt COMPONENT Mod)
install(FILES "${PHI_ZYDIS_SOURCE_DIR}/LICENSE" DESTINATION PirateHatHUD
  RENAME LICENSE-Zydis.txt COMPONENT Mod)
set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "PirateHatHUD-${PROJECT_VERSION}")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_SOURCE_DIR}/dist")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_INSTALL_CMAKE_PROJECTS "${CMAKE_BINARY_DIR};PirateHatHUD;Mod;/")
include(CPack)

include(CTest)
if(BUILD_TESTING)
  add_test(NAME packaging_tests COMMAND ${CMAKE_COMMAND}
    "-DSTAGE_DIR=$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD"
    "-DEXPECTED_VERSION=${PROJECT_VERSION}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PackagingTests.cmake")
  add_test(NAME architecture_check COMMAND ${CMAKE_COMMAND}
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Architecture.cmake")
  add_test(NAME architecture_checker_tests COMMAND ${CMAKE_COMMAND}
    "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/architecture-tests"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/ArchitectureTests.cmake")
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
  add_executable(menu_tests tests/menu_tests.cpp)
  target_link_libraries(menu_tests PRIVATE game_observers)
  add_test(NAME menu_tests COMMAND menu_tests)
  add_executable(minimap_tests tests/minimap_tests.cpp)
  target_link_libraries(minimap_tests PRIVATE game_observers)
  add_test(NAME minimap_tests COMMAND minimap_tests)
  add_executable(memory_reader_tests tests/memory_reader_tests.cpp)
  target_link_libraries(memory_reader_tests PRIVATE game_observers)
  target_compile_definitions(memory_reader_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  add_test(NAME memory_reader_tests COMMAND memory_reader_tests)
  add_executable(observer_hook_tests tests/observer_hook_tests.cpp)
  target_include_directories(observer_hook_tests PRIVATE include)
  add_test(NAME observer_hook_tests COMMAND observer_hook_tests)
  add_executable(hotkey_tests tests/hotkey_tests.cpp)
  target_include_directories(hotkey_tests PRIVATE include)
  add_test(NAME hotkey_tests COMMAND hotkey_tests)
  add_executable(logger_tests tests/logger_tests.cpp src/platform/logger.cpp src/platform/config.cpp)
  target_include_directories(logger_tests PRIVATE include)
  target_compile_definitions(logger_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  add_test(NAME logger_tests COMMAND logger_tests)
  add_executable(app_tests tests/app_tests.cpp src/app.cpp)
  target_link_libraries(app_tests PRIVATE treasure_indicator)
  target_compile_definitions(app_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX
    PHI_VERSION="${PROJECT_VERSION}")
  add_test(NAME app_tests COMMAND app_tests)
  add_executable(render_wait_tests tests/render_wait_tests.cpp)
  target_include_directories(render_wait_tests PRIVATE include)
  add_test(NAME render_wait_tests COMMAND render_wait_tests)
  add_executable(swapchain_tests tests/swapchain_tests.cpp)
  target_include_directories(swapchain_tests PRIVATE include)
  add_test(NAME swapchain_tests COMMAND swapchain_tests)
  add_executable(render_icon_tests tests/render_icon_tests.cpp)
  target_include_directories(render_icon_tests PRIVATE include)
  target_link_libraries(render_icon_tests PRIVATE imgui_static)
  add_test(NAME render_icon_tests COMMAND render_icon_tests)
  add_executable(hdr_shader_tests tests/hdr_shader_tests.cpp src/render/hdr_bytecode.cpp)
  target_include_directories(hdr_shader_tests PRIVATE include)
  target_link_libraries(hdr_shader_tests PRIVATE d3dcompiler)
  add_test(NAME hdr_shader_tests COMMAND hdr_shader_tests)
  add_executable(image_tests tests/image_tests.cpp src/render/image.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/generated/icon.rc")
  target_include_directories(image_tests PRIVATE include)
  target_compile_definitions(image_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(image_tests PRIVATE ole32 windowscodecs)
  add_test(NAME image_tests COMMAND image_tests "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.png")
endif()

phi_check_target(game_observers safetyhook)
phi_check_target(treasure_indicator)
phi_check_target(PirateHatHUD game_observers treasure_indicator safetyhook
  imgui_static d3d12 dxgi d3dcompiler ole32 windowscodecs)
