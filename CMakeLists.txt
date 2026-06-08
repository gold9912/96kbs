cmake_minimum_required(VERSION 3.24)

project(Rogue96DXR
    VERSION 0.1.0
    DESCRIPTION "Tiny procedural D3D12/DXR roguelike prototype"
    LANGUAGES CXX)

option(ROGUE96_BUILD_TESTS "Build deterministic gameplay tests" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()

find_package(Python3 COMPONENTS Interpreter REQUIRED)

if(NOT MSVC)
    message(FATAL_ERROR
        "Rogue96DXR is intentionally MSVC/Windows SDK first because D3D12/DXR "
        "and embedded DXIL are part of the architecture. Open a Developer "
        "PowerShell for VS 2022 or configure CMake with an MSVC generator.")
endif()

find_program(DXC_EXECUTABLE dxc)
if(NOT DXC_EXECUTABLE)
    message(FATAL_ERROR
        "dxc.exe was not found. Install the Windows SDK or DirectX Shader "
        "Compiler and ensure dxc is available in PATH for offline DXIL embedding.")
endif()

set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(SHADER_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${GENERATED_DIR}" "${SHADER_BIN_DIR}")

set(SHADER_WORLD_VS "${SHADER_BIN_DIR}/fullscreen_vs.dxil")
set(SHADER_WORLD_PS "${SHADER_BIN_DIR}/world_fullscreen.dxil")
set(SHADER_COMPOSITE_PS "${SHADER_BIN_DIR}/dxr_composite.dxil")
set(SHADER_DXR_LIB "${SHADER_BIN_DIR}/dxr_entities.dxil")
set(EMBEDDED_SHADER_HEADER "${GENERATED_DIR}/embedded_shaders.h")
set(EMBEDDED_SHADER_SOURCE "${GENERATED_DIR}/embedded_shaders.cpp")

add_custom_command(
    OUTPUT "${SHADER_WORLD_VS}"
    COMMAND "${DXC_EXECUTABLE}" -nologo -E MainVS -T vs_6_0
            -Fo "${SHADER_WORLD_VS}" "${SHADER_SOURCE_DIR}/fullscreen_vs.hlsl"
    DEPENDS "${SHADER_SOURCE_DIR}/fullscreen_vs.hlsl"
    VERBATIM)

add_custom_command(
    OUTPUT "${SHADER_WORLD_PS}"
    COMMAND "${DXC_EXECUTABLE}" -nologo -E MainPS -T ps_6_0
            -Fo "${SHADER_WORLD_PS}" "${SHADER_SOURCE_DIR}/world_fullscreen.hlsl"
    DEPENDS "${SHADER_SOURCE_DIR}/world_fullscreen.hlsl"
    VERBATIM)

add_custom_command(
    OUTPUT "${SHADER_COMPOSITE_PS}"
    COMMAND "${DXC_EXECUTABLE}" -nologo -E MainPS -T ps_6_0
            -Fo "${SHADER_COMPOSITE_PS}" "${SHADER_SOURCE_DIR}/dxr_composite.hlsl"
    DEPENDS "${SHADER_SOURCE_DIR}/dxr_composite.hlsl"
    VERBATIM)

add_custom_command(
    OUTPUT "${SHADER_DXR_LIB}"
    COMMAND "${DXC_EXECUTABLE}" -nologo -T lib_6_3
            -Fo "${SHADER_DXR_LIB}" "${SHADER_SOURCE_DIR}/dxr_entities.hlsl"
    DEPENDS "${SHADER_SOURCE_DIR}/dxr_entities.hlsl"
    VERBATIM)

add_custom_command(
    OUTPUT "${EMBEDDED_SHADER_HEADER}" "${EMBEDDED_SHADER_SOURCE}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_binary.py"
            --header "${EMBEDDED_SHADER_HEADER}"
            --source "${EMBEDDED_SHADER_SOURCE}"
            --namespace "rogue::shaders"
            "FullscreenVS=${SHADER_WORLD_VS}"
            "WorldFullscreenPS=${SHADER_WORLD_PS}"
            "DxrCompositePS=${SHADER_COMPOSITE_PS}"
            "DxrEntities=${SHADER_DXR_LIB}"
    DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_binary.py"
            "${SHADER_WORLD_VS}"
            "${SHADER_WORLD_PS}"
            "${SHADER_COMPOSITE_PS}"
            "${SHADER_DXR_LIB}"
    VERBATIM)

set(ROGUE96_GAME_SOURCES
    src/game/combat_sim.cpp
    src/game/game_session.cpp
    src/game/world_gen.cpp
    src/render/entity_rt_proxy.cpp
    src/render/render_scene.cpp
    src/render/visual_style.cpp)

set(ROGUE96_APP_SOURCES
    src/app/app.cpp
    src/platform/d3d12_context.cpp
    src/render/dxr_scene_resources.cpp
    src/render/dxr_pipeline.cpp
    src/render/dxr_shader_table.cpp
    src/render/gpu_upload_buffer.cpp
    src/win32_main.cpp)

set(ROGUE96_TEST_SOURCES
    tests/sim_tests.cpp)

set(ROGUE96_SHARED_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game/combat_sim.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game/game_session.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game/math.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game/rng.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game/world_gen.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/render/entity_rt_proxy.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/render/render_scene.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/render/visual_style.h")

set_source_files_properties(
    ${ROGUE96_GAME_SOURCES}
    ${ROGUE96_APP_SOURCES}
    ${ROGUE96_TEST_SOURCES}
    PROPERTIES OBJECT_DEPENDS "${ROGUE96_SHARED_HEADERS}")

add_library(rogue96_core STATIC ${ROGUE96_GAME_SOURCES})
target_include_directories(rogue96_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(rogue96_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)

add_executable(rogue96 WIN32
    ${ROGUE96_APP_SOURCES}
    "${EMBEDDED_SHADER_SOURCE}")

target_include_directories(rogue96 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${GENERATED_DIR}")
target_compile_definitions(rogue96 PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(rogue96 PRIVATE
    rogue96_core
    d3d12
    dxgi
    dxguid
    user32
    gdi32)

if(MSVC)
    target_compile_options(rogue96 PRIVATE /W4 /permissive-)
    target_compile_options(rogue96_core PRIVATE /W4 /permissive-)
    target_compile_options(rogue96 PRIVATE
        $<$<CONFIG:Release>:/O1 /GL /Gy /Gw>)
    target_link_options(rogue96 PRIVATE
        $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF /INCREMENTAL:NO>)
endif()

add_custom_target(size_report
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/size_report.py"
            --max-mb 15 "$<TARGET_FILE:rogue96>"
    DEPENDS rogue96
    VERBATIM)

if(ROGUE96_BUILD_TESTS)
    add_executable(rogue96_tests
        ${ROGUE96_TEST_SOURCES})
    target_include_directories(rogue96_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(rogue96_tests PRIVATE rogue96_core)
    add_test(NAME rogue96_tests COMMAND rogue96_tests)
endif()
