cmake_minimum_required(VERSION 3.16)
project(BmoHandheldHostTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BMO_ENABLE_ASAN_UBSAN "Enable AddressSanitizer and UndefinedBehaviorSanitizer on supported compilers" OFF)

enable_testing()

function(bmo_add_host_test target_name)
  add_executable(${target_name} ${ARGN})
  target_include_directories(${target_name} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/firmware/BmoGameboy"
    "${CMAKE_CURRENT_SOURCE_DIR}/firmware/BmoGameboy/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/firmware/BmoGameboy/src/app"
    "${CMAKE_CURRENT_SOURCE_DIR}/firmware/BmoGameboy/src/hal"
    "${CMAKE_CURRENT_SOURCE_DIR}/firmware/BmoGameboy/src/storage"
    "${CMAKE_CURRENT_SOURCE_DIR}/firmware/BmoGameboy/src/ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/firmware/BmoGameboy/src/cores"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/host/fakes"
  )
  target_compile_definitions(${target_name} PRIVATE BMO_HOST_TEST=1)
  if(MSVC)
    target_compile_definitions(${target_name} PRIVATE _CRT_SECURE_NO_WARNINGS)
    # Keep assertions active even in Release builds so no test silently skips checks.
    target_compile_options(${target_name} PRIVATE /W4 /WX /permissive- /UNDEBUG)
  else()
    target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wshadow -Werror -UNDEBUG)
    if(BMO_ENABLE_ASAN_UBSAN)
      target_compile_options(${target_name} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -g)
      target_link_options(${target_name} PRIVATE -fsanitize=address,undefined)
    endif()
  endif()
endfunction()

function(bmo_add_host_exec target_name)
  bmo_add_host_test(${target_name} ${ARGN})
  add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

bmo_add_host_exec(battery_save_test tests/host/battery_save_test.cpp tests/host/static_check_main.cpp)
bmo_add_host_exec(box_art_lookup_test tests/host/box_art_lookup_test.cpp)
bmo_add_host_exec(frame_pacing_test tests/host/frame_pacing_test.cpp)
bmo_add_host_exec(frame_percentiles_test tests/host/frame_percentiles_test.cpp)
bmo_add_host_exec(harmonic_spring_test tests/host/harmonic_spring_test.cpp)
bmo_add_host_exec(pause_quit_policy_test tests/host/pause_quit_policy_test.cpp)
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
  bmo_add_host_exec(psram_zone_budget_test tests/host/psram_zone_budget_test.cpp)
endif()
bmo_add_host_exec(rom_file_filter_test tests/host/rom_file_filter_test.cpp)
bmo_add_host_exec(rom_index_layout_test tests/host/rom_index_layout_test.cpp tests/host/static_check_main.cpp)
bmo_add_host_exec(rom_key_parity_test tests/host/rom_key_parity.cpp)

bmo_add_host_test(spi_arbiter_test tests/host/spi_arbiter_test.cpp firmware/BmoGameboy/src/hal/spi_arbiter.cpp)
add_test(NAME spi_arbiter_test COMMAND spi_arbiter_test ok)

bmo_add_host_exec(theme_color_test tests/host/theme_color_test.cpp)
bmo_add_host_exec(boot_safe_mode_test tests/host/boot_safe_mode_test.cpp)
bmo_add_host_exec(input_frame_test tests/host/input_frame_test.cpp)
bmo_add_host_exec(console_catalog_test tests/host/console_catalog_test.cpp)
bmo_add_host_exec(bmo_face_sdf_test tests/host/bmo_face_sdf_test.cpp)
bmo_add_host_exec(atomic_fs_test tests/host/atomic_fs_test.cpp)
bmo_add_host_exec(core_registry_test tests/host/core_registry_test.cpp)
