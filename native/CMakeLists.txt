cmake_minimum_required(VERSION 3.20)

# ============================================================================
# NHL Hitz 2003 - Static Recompilation
#
# Copy this template to your game project root and customize:
#   1. Change the project name below
#   2. Set XBOXRECOMP_DIR to where you cloned xboxrecomp
#   3. Adjust source file lists as you add game-specific code
#   4. Add any game-specific compile definitions or link libraries
# ============================================================================

project(nhl_hitz_2003_recomp C)
if(UNIX AND NOT APPLE)
    include(cmake/linux.cmake)
    return()
endif()
option(NHL_RELEASE "Packaged player build with user-writable data paths and quiet output" OFF)
option(HITZ_DEBUGKIT "Debug build with the xrdbg self-monitoring kit" OFF)
set(HITZ_PACKAGE_VERSION "1.0.0" CACHE STRING "Installer and executable version")
if(NHL_RELEASE AND NOT HITZ_DEBUGKIT)
    set(HITZ_PLAYER_BUILD ON)
    add_compile_definitions(HITZ_PLAYER_BUILD=1)
    if(MSVC)
        add_compile_options("$<$<COMPILE_LANGUAGE:C>:/FI${CMAKE_CURRENT_SOURCE_DIR}/src/player_environment.h>")
    else()
        add_compile_options("$<$<COMPILE_LANGUAGE:C>:-include${CMAKE_CURRENT_SOURCE_DIR}/src/player_environment.h>")
    endif()
endif()
if(NOT HITZ_PACKAGE_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "HITZ_PACKAGE_VERSION must have three numeric components")
endif()
string(REPLACE "." "," HITZ_FILE_VERSION "${HITZ_PACKAGE_VERSION}")
if(NHL_RELEASE AND MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()

add_executable(hitz_ffp_material_test EXCLUDE_FROM_ALL tests/ffp_material_test.c)
add_executable(hitz_player_environment_test EXCLUDE_FROM_ALL tests/player_environment_test.c)
target_include_directories(hitz_ffp_material_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/overrides")
add_executable(hitz_ffp_lighting_test EXCLUDE_FROM_ALL tests/ffp_lighting_test.c)
target_include_directories(hitz_ffp_lighting_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/overrides")
add_executable(hitz_rt_format_test EXCLUDE_FROM_ALL tests/rt_format_test.c)
target_link_libraries(hitz_rt_format_test PRIVATE d3d11 d3dcompiler dxgi dxguid)
add_executable(hitz_nis_lifetime_test EXCLUDE_FROM_ALL tests/nis_lifetime_test.c src/hitz_nis.c)
add_executable(hitz_ffp_gpu_math_test EXCLUDE_FROM_ALL tests/ffp_gpu_math_test.c)
target_link_libraries(hitz_ffp_gpu_math_test PRIVATE d3d11 d3dcompiler)
add_executable(hitz_shader_prepare tools/shader_prepare.c)
target_link_libraries(hitz_shader_prepare PRIVATE d3dcompiler)
add_executable(hitz_shader_cache_test EXCLUDE_FROM_ALL tests/shader_cache_test.c)
target_link_libraries(hitz_shader_cache_test PRIVATE d3dcompiler)
add_executable(hitz_bind_cache_test EXCLUDE_FROM_ALL tests/bind_cache_test.c)
target_link_libraries(hitz_bind_cache_test PRIVATE d3d11 d3dcompiler)
add_executable(hitz_bulk_append_test EXCLUDE_FROM_ALL tests/bulk_append_test.c)
add_executable(hitz_texture_hash_test EXCLUDE_FROM_ALL tests/texture_hash_test.c)
add_executable(hitz_disc_buffering_test EXCLUDE_FROM_ALL tests/disc_buffering_test.c)
add_executable(hitz_vsh_export_test EXCLUDE_FROM_ALL tests/vsh_export_test.c)
target_include_directories(hitz_vsh_export_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../tools/xboxrecomp/include" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/xboxrecomp/src/kernel")
add_executable(hitz_pb_scan_bulk_test EXCLUDE_FROM_ALL tests/pb_scan_bulk_test.c)
add_executable(hitz_draw_batch_test EXCLUDE_FROM_ALL tests/draw_batch_test.c)
target_link_libraries(hitz_draw_batch_test PRIVATE d3d11 d3dcompiler)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Common definitions
add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

# ── xboxrecomp toolkit ──────────────────────────────────────────
#
# Option A: xboxrecomp as a subdirectory (simplest)
#   Place xboxrecomp/ next to your game project and use a relative path.
#
# Option B: find_package (if you install xboxrecomp system-wide)
#   find_package(xboxrecomp REQUIRED)
#
# Adjust this path to point to your xboxrecomp clone:
set(XBOXRECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../tools/xboxrecomp" CACHE PATH
    "Path to the xboxrecomp toolkit root directory")

# Add xboxrecomp as a subdirectory -- this builds all 6 runtime libraries
# and provides the 'xboxrecomp' umbrella target to link against.
add_subdirectory(${XBOXRECOMP_DIR} ${CMAKE_BINARY_DIR}/xboxrecomp)

# Reviewed runtime components adapted from the working sibling Hitz Pro port.
# Keep the toolkit clean and compile replacements in this project.
function(hitz_runtime_overrides target)
    get_target_property(sources ${target} SOURCES)
    foreach(name IN LISTS ARGN)
        list(REMOVE_ITEM sources ${name})
    endforeach()
    set_property(TARGET ${target} PROPERTY SOURCES "${sources}")
    foreach(name IN LISTS ARGN)
        target_sources(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/overrides/${name}")
    endforeach()
endfunction()
hitz_runtime_overrides(xbox_kernel nv2a_pb_exec.c nv2a_pb_scan.c
    nv2a_vsh_interp.c nv2a_combiner.c kernel_bridge.c kernel_hal.c xbox_memory_layout.c kernel_path.c kernel_file.c)
hitz_runtime_overrides(host_time host_time.c)
hitz_runtime_overrides(xbox_apu apu_xaudio2.c)
# JOJO item 15 (experimental, off by default): the Windows build's APU voice processor override.
option(HITZ_LINUX_APU_VP "Build overrides/apu_vp.c on Linux like Windows" OFF)
if(HITZ_LINUX_APU_VP)
    hitz_runtime_overrides(xbox_apu apu_vp.c)
endif()
hitz_runtime_overrides(xbox_usb usb_gamepad.c ohci.c)
hitz_runtime_overrides(xbox_input keyboard.c)
target_include_directories(xbox_input PRIVATE "${XBOXRECOMP_DIR}/src/input")
if(NHL_RELEASE)
    hitz_runtime_overrides(xbox_kernel kernel_thunks.c)
    target_compile_definitions(xbox_kernel PRIVATE NHL_RELEASE=1)
endif()
target_include_directories(host_time PRIVATE "${XBOXRECOMP_DIR}/src/platform")
target_include_directories(xbox_apu PRIVATE "${XBOXRECOMP_DIR}/src/apu")
target_include_directories(xbox_usb PRIVATE "${XBOXRECOMP_DIR}/src/usb")
target_include_directories(xbox_kernel PRIVATE "${XBOXRECOMP_DIR}/src")

add_executable(hitz_virtual_disk_test EXCLUDE_FROM_ALL tests/virtual_disk_test.c)
target_include_directories(hitz_virtual_disk_test PRIVATE "${XBOXRECOMP_DIR}/src/kernel")
target_link_libraries(hitz_virtual_disk_test PRIVATE xboxrecomp)

# ── Recompiled game executable ──────────────────────────────────

# Collect generated recompiled code.
# The recomp pipeline outputs files into src/recomp/gen/.
# With --split 1000, you get gen_0000.c, gen_0001.c, etc.
# CONFIGURE_DEPENDS so re-running the recompiler picks up added/removed files
# without a manual re-configure. A plain GLOB is evaluated once at configure
# time, so a newly generated file (an extra chunk, recomp_stubs_unresolved.c)
# is silently left out of the build and shows up as unresolved externals.
file(GLOB RECOMP_GEN_SOURCES CONFIGURE_DEPENDS "src/recomp/gen/*.c")

# Keep direct calls routed through the project-owned NIS lifetime fix without
# editing generated sources. Retain the original body as a renamed reference.
set(nis_generated "${CMAKE_CURRENT_SOURCE_DIR}/src/recomp/gen/recomp_0029.c")
if(EXISTS "${nis_generated}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${nis_generated}")
    file(READ "${nis_generated}" nis_source)
    string(REPLACE "void sub_00147AE0(void)\n{" "void hitz_original_00147AE0(void)\n{" nis_source "${nis_source}")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/recomp_nis.c" "${nis_source}")
    list(REMOVE_ITEM RECOMP_GEN_SOURCES "${nis_generated}")
    list(APPEND RECOMP_GEN_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/recomp_nis.c")
endif()

# Core game sources
set(GAME_SOURCES
    src/main.c
    src/recomp_manual.c
    src/hitz_nis.c
    # Add game-specific sources here, e.g.:
    # src/asset_loader.c
    # src/custom_renderer.c
)

# Compile graphics independently during runtime bring-up; no generated guest
# sources are involved in this target.
add_library(hitz_renderer OBJECT src/backend_d3d11.c)
target_include_directories(hitz_renderer PRIVATE
    "${XBOXRECOMP_DIR}/src" "${XBOXRECOMP_DIR}/src/kernel")
if(MSVC AND NOT HITZ_PLAYER_BUILD)
    target_compile_options(hitz_renderer PRIVATE /Zi)
endif()

# The recomp pipeline also generates a dispatch header and function declarations.
# These are included by the generated .c files:
#   src/recomp/gen/recomp_funcs.h    - extern declarations for all recompiled functions
#   src/recomp/recomp_dispatch.h     - binary search dispatch table
#   src/recomp/recomp_types.h        - register model, ICALL macros, MEM helpers

add_executable(${PROJECT_NAME} WIN32
    ${GAME_SOURCES}
    ${RECOMP_GEN_SOURCES}
)

if(WIN32)
    enable_language(RC)
    configure_file(resources/game.rc.in ${CMAKE_CURRENT_BINARY_DIR}/game.rc @ONLY)
    configure_file(resources/config.rc.in ${CMAKE_CURRENT_BINARY_DIR}/config.rc @ONLY)
    target_sources(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/game.rc)
    target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources")
    add_executable(nhl_import installer/nhl_import.c)
    target_link_libraries(nhl_import PRIVATE bcrypt shell32)
    add_executable(nhl_config WIN32 config/config.c ${CMAKE_CURRENT_BINARY_DIR}/config.rc)
    target_include_directories(nhl_config PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources")
    target_link_libraries(nhl_config PRIVATE comctl32 user32 gdi32 shell32)
    set_target_properties(nhl_config PROPERTIES OUTPUT_NAME config)
    add_executable(hitz_player_settings_test EXCLUDE_FROM_ALL tests/player_settings_test.c)
    target_include_directories(hitz_player_settings_test PRIVATE "${XBOXRECOMP_DIR}/src/input" "${XBOXRECOMP_DIR}/include")
    target_link_libraries(hitz_player_settings_test PRIVATE xbox_input_map comctl32 user32 gdi32 shell32)
    if(MSVC)
        set_property(TARGET nhl_import nhl_config PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
    endif()
endif()
if(NHL_RELEASE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE NHL_RELEASE=1)
    target_compile_definitions(hitz_renderer PRIVATE NHL_RELEASE=1)
endif()

# Link against the xboxrecomp umbrella target (all 6 runtime libraries)
target_link_libraries(${PROJECT_NAME} PRIVATE xboxrecomp hitz_renderer)

# Include paths for generated recomp headers
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recomp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/recomp/gen
    ${XBOXRECOMP_DIR}/include
)

# Windows SDK libraries needed at link time
target_link_libraries(${PROJECT_NAME} PRIVATE
    d3d11
    dxgi
    d3dcompiler
    windowscodecs
    ole32
    uuid
    dxguid
    $<IF:$<BOOL:${MINGW}>,xinput1_4,xinput>   # see src/input/CMakeLists.txt
    winmm
    # Lets the fault handler resolve the host RIP back to the generated
    # function's symbol, i.e. to the guest function that faulted.
    $<$<NOT:$<BOOL:${HITZ_PLAYER_BUILD}>>:dbghelp>
)

# MSVC-specific: large address aware, increase section limit for big generated files
if(MSVC)
    # Keep optimized runtime function symbols: otherwise sampling resolves
    # static graphics routines to unrelated neighboring public symbols.
    target_link_options(${PROJECT_NAME} PRIVATE
        /LARGEADDRESSAWARE
        /OPT:REF /OPT:ICF
    )
    # The generated code files can be very large. Increase object file section limit.
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj /MP4)
    if(HITZ_PLAYER_BUILD)
        target_link_options(${PROJECT_NAME} PRIVATE /DEBUG:NONE)
    else()
        target_compile_options(xbox_kernel PRIVATE /Zi)
        target_compile_options(${PROJECT_NAME} PRIVATE /Zi)
        target_link_options(${PROJECT_NAME} PRIVATE /DEBUG)
    endif()
endif()

# ── Debug kit (xrdbg) ───────────────────────────────────────────
#
# Self-monitoring debug build: session log, crash/hang/mark minidumps with
# guest RAM, DEL marks a glitch (screenshots), controller log, PDB. See
# DEBUG-KIT.md. Never on in a normal build; configure it in its own directory:
#   cmake -S native -B native/build-debug -G "Visual Studio 17 2022" -A x64 -DHITZ_DEBUGKIT=ON
set(HITZ_DEBUG_TAG "debug" CACHE STRING "Build tag printed in the debug log header and banner")
if(HITZ_DEBUGKIT)
    set(XRDBG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../tools/xrdbg")
    include(${XRDBG_DIR}/cmake/xrdbg.cmake)
    xrdbg_enable(${PROJECT_NAME})
    target_sources(${PROJECT_NAME} PRIVATE src/hitz_debug.c)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HITZ_DEBUG_TAG="${HITZ_DEBUG_TAG}")
    # Hook sites outside the exe target: the renderer (frame count, DEL,
    # screenshots, quit paths) and kernel_hal.c (bugcheck / return to dashboard).
    foreach(hook_target IN ITEMS hitz_renderer xbox_kernel)
        target_compile_definitions(${hook_target} PRIVATE XRDBG=1)
        target_include_directories(${hook_target} PRIVATE "${XRDBG_DIR}/include")
    endforeach()
endif()

# ── Game-specific additions ─────────────────────────────────────
#
# Add any game-specific libraries, loaders, or custom renderers below.
# Examples:
#
# # Custom texture loader for your game's format
# target_sources(${PROJECT_NAME} PRIVATE src/texture_loader.c)
#
# # If your game uses RenderWare, you may need a RW bridge layer
# target_sources(${PROJECT_NAME} PRIVATE src/rw_bridge.c)
#
# # Additional compile definitions for debugging
# target_compile_definitions(${PROJECT_NAME} PRIVATE
#     ENABLE_ICALL_TRACE=1
#     VERBOSE_KERNEL_LOG=1
# )
