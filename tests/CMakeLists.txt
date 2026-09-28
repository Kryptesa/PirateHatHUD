# Test registration stays separate from production targets.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(NAME release_validation_tests COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/tooling/release_validation_tests.py")
add_test(NAME format_cpp_tests COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/tooling/format_cpp_tests.py")
add_test(NAME packaging_tests COMMAND ${CMAKE_COMMAND}
  "-DSTAGE_DIR=$<TARGET_FILE_DIR:PirateHatHUD>/PirateHatHUD"
  "-DEXPECTED_VERSION=${PROJECT_VERSION}"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tooling/packaging_tests.cmake")
add_test(NAME architecture_check COMMAND ${CMAKE_COMMAND}
  -P "${PROJECT_SOURCE_DIR}/cmake/Architecture.cmake")
add_test(NAME architecture_checker_tests COMMAND ${CMAKE_COMMAND}
  "-DTEST_ROOT=${PROJECT_BINARY_DIR}/architecture-tests"
  -P "${CMAKE_CURRENT_SOURCE_DIR}/tooling/architecture_checker_tests.cmake")

# Core
add_executable(events_tests core/events_tests.cpp)
target_include_directories(events_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
add_test(NAME events_tests COMMAND events_tests)

set_tests_properties(
  events_tests
  PROPERTIES LABELS core)

# Game
add_executable(scanner_tests
  game/scanner_tests.cpp
  "${PROJECT_SOURCE_DIR}/src/game/shared/address.cpp"
  "${PROJECT_SOURCE_DIR}/src/game/treasure/scan.cpp"
  "${PROJECT_SOURCE_DIR}/src/game/menu/scan.cpp"
  "${PROJECT_SOURCE_DIR}/src/game/shared/ui_root_scan.cpp"
  "${PROJECT_SOURCE_DIR}/src/pattern_scan.cpp")
target_include_directories(scanner_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(scanner_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
add_test(NAME scanner_tests COMMAND scanner_tests)

add_executable(game_address_tests
  game/game_address_tests.cpp
  "${PROJECT_SOURCE_DIR}/src/game/shared/address.cpp")
target_include_directories(game_address_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
add_test(NAME game_address_tests COMMAND game_address_tests)

add_executable(treasure_memory_tests game/treasure_memory_tests.cpp)
target_link_libraries(treasure_memory_tests PRIVATE game_observers)
add_test(NAME treasure_memory_tests COMMAND treasure_memory_tests)

add_executable(treasure_range_tests game/treasure_range_tests.cpp)
target_link_libraries(treasure_range_tests PRIVATE game_observers)
target_compile_definitions(treasure_range_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
add_test(NAME treasure_range_tests COMMAND treasure_range_tests)

add_executable(audio_volume_tests game/audio_volume_tests.cpp)
target_link_libraries(audio_volume_tests PRIVATE game_observers)
add_test(NAME audio_volume_tests COMMAND audio_volume_tests)

add_executable(observer_tests game/observer_tests.cpp)
target_link_libraries(observer_tests PRIVATE game_observers)
target_compile_definitions(observer_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
add_test(NAME observer_tests COMMAND observer_tests)

add_executable(menu_tests game/menu_tests.cpp)
target_link_libraries(menu_tests PRIVATE game_observers)
add_test(NAME menu_tests COMMAND menu_tests)

add_executable(minimap_tests game/minimap_tests.cpp)
target_link_libraries(minimap_tests PRIVATE game_observers)
add_test(NAME minimap_tests COMMAND minimap_tests)

add_executable(memory_reader_tests game/memory_reader_tests.cpp)
target_link_libraries(memory_reader_tests PRIVATE game_observers)
target_compile_definitions(memory_reader_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
add_test(NAME memory_reader_tests COMMAND memory_reader_tests)

add_executable(observer_hook_tests game/observer_hook_tests.cpp)
target_include_directories(observer_hook_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
add_test(NAME observer_hook_tests COMMAND observer_hook_tests)

set_tests_properties(
  scanner_tests
  game_address_tests
  treasure_memory_tests
  treasure_range_tests
  audio_volume_tests
  observer_tests
  menu_tests
  minimap_tests
  memory_reader_tests
  observer_hook_tests
  PROPERTIES LABELS game)

# Features
add_executable(indicator_tests features/indicator_tests.cpp)
target_link_libraries(indicator_tests PRIVATE treasure_indicator)
add_test(NAME indicator_tests COMMAND indicator_tests)

add_executable(treasure_sound_tests features/treasure_sound_tests.cpp)
target_link_libraries(treasure_sound_tests PRIVATE treasure_indicator)
add_test(NAME treasure_sound_tests COMMAND treasure_sound_tests)

set_tests_properties(
  indicator_tests
  treasure_sound_tests
  PROPERTIES LABELS features)

# Platform
add_executable(wave_pcm_tests
  platform/wave_pcm_tests.cpp
  "${PROJECT_SOURCE_DIR}/src/platform/wave_pcm.cpp")
target_include_directories(wave_pcm_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
add_test(NAME wave_pcm_tests COMMAND wave_pcm_tests)

add_executable(sound_tests platform/sound_tests.cpp "${PROJECT_SOURCE_DIR}/src/platform/sound.cpp" "${PROJECT_SOURCE_DIR}/src/platform/wave_pcm.cpp"
  "${PROJECT_BINARY_DIR}/generated/icon.rc")
target_include_directories(sound_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(sound_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(sound_tests PRIVATE winmm)
add_test(NAME sound_tests COMMAND sound_tests)

add_executable(hotkey_tests platform/hotkey_tests.cpp)
target_include_directories(hotkey_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
add_test(NAME hotkey_tests COMMAND hotkey_tests)

add_executable(logger_tests
  platform/logger_tests.cpp
  "${PROJECT_SOURCE_DIR}/src/platform/logger.cpp"
  "${PROJECT_SOURCE_DIR}/src/platform/config.cpp")
target_include_directories(logger_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(logger_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
add_test(NAME logger_tests COMMAND logger_tests)

set_tests_properties(
  wave_pcm_tests
  sound_tests
  hotkey_tests
  logger_tests
  PROPERTIES LABELS platform)

# Render
set(PHI_RENDER_TEST_SOURCES
  "${PROJECT_SOURCE_DIR}/src/render/dx12_renderer.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/dx12_frames.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/dx12_texture.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/dx12_hdr.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/hdr_bytecode.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/hud_draw.cpp")
add_executable(resize_tests render/resize_tests.cpp ${PHI_RENDER_TEST_SOURCES})
target_include_directories(resize_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(resize_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(resize_tests PRIVATE imgui_static d3d12 dxgi d3dcompiler)
add_test(NAME resize_tests COMMAND resize_tests)

add_executable(dxgi_lifecycle_tests render/dxgi_lifecycle_tests.cpp
  ${PHI_RENDER_TEST_SOURCES}
  "${PROJECT_SOURCE_DIR}/src/render/dxgi_hooks.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/dxgi_probe.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/dxgi_swapchains.cpp"
  "${PROJECT_SOURCE_DIR}/src/render/dxgi_diagnostics.cpp")
target_include_directories(dxgi_lifecycle_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(dxgi_lifecycle_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(dxgi_lifecycle_tests PRIVATE imgui_static safetyhook d3d12 dxgi d3dcompiler)
add_test(NAME dxgi_lifecycle_tests COMMAND dxgi_lifecycle_tests)

add_executable(render_wait_tests render/render_wait_tests.cpp)
target_include_directories(render_wait_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
add_test(NAME render_wait_tests COMMAND render_wait_tests)

add_executable(swapchain_tests render/swapchain_tests.cpp)
target_include_directories(swapchain_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
add_test(NAME swapchain_tests COMMAND swapchain_tests)

add_executable(render_icon_tests render/render_icon_tests.cpp)
target_include_directories(render_icon_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(render_icon_tests PRIVATE imgui_static)
add_test(NAME render_icon_tests COMMAND render_icon_tests)

add_executable(hdr_shader_tests
  render/hdr_shader_tests.cpp
  "${PROJECT_SOURCE_DIR}/src/render/hdr_bytecode.cpp")
target_include_directories(hdr_shader_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(hdr_shader_tests PRIVATE d3dcompiler)
add_test(NAME hdr_shader_tests COMMAND hdr_shader_tests)

add_executable(image_tests render/image_tests.cpp "${PROJECT_SOURCE_DIR}/src/render/image.cpp"
  "${PROJECT_BINARY_DIR}/generated/icon.rc")
target_include_directories(image_tests PRIVATE "${PROJECT_SOURCE_DIR}/include")
target_compile_definitions(image_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(image_tests PRIVATE ole32 windowscodecs)
add_test(NAME image_tests COMMAND image_tests "${PROJECT_SOURCE_DIR}/assets/icon.png")

set_tests_properties(
  resize_tests
  dxgi_lifecycle_tests
  render_wait_tests
  swapchain_tests
  render_icon_tests
  hdr_shader_tests
  image_tests
  PROPERTIES LABELS render)

# App
add_executable(app_tests app/app_tests.cpp "${PROJECT_SOURCE_DIR}/src/app.cpp")
target_link_libraries(app_tests PRIVATE treasure_indicator version)
target_compile_definitions(app_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX
  PHI_VERSION="${PROJECT_VERSION}")
add_test(NAME app_tests COMMAND app_tests)

set_tests_properties(
  app_tests
  PROPERTIES LABELS app)

set_tests_properties(
  release_validation_tests
  format_cpp_tests
  packaging_tests
  architecture_check
  architecture_checker_tests
  PROPERTIES LABELS tooling)
