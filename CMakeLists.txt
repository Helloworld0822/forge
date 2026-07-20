cmake_minimum_required(VERSION 3.16)
project(forge VERSION 0.3.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_library(forge_std STATIC
    stdlib/io.c
    stdlib/string.c
    stdlib/math.c
    stdlib/time.c
    stdlib/fs.c
    stdlib/os.c
    stdlib/tcp.c
    stdlib/udp.c
    stdlib/http.c
    stdlib/json.c
    runtime/arena.c
)
target_include_directories(forge_std PUBLIC include)
if(NOT WIN32)
    target_link_libraries(forge_std PUBLIC pthread)
endif()

add_library(forge_runtime STATIC
    runtime/runtime.c
    runtime/scheduler.c
    runtime/work_queue.c
    runtime/event_loop.c
    runtime/ownership.c
    runtime/platform.c
    runtime/thread.c
)
if(WIN32)
    target_link_libraries(forge_runtime PUBLIC forge_std ws2_32)
else()
    target_link_libraries(forge_runtime PUBLIC forge_std pthread)
endif()
target_include_directories(forge_runtime PUBLIC include runtime)

add_executable(forge
    compiler/main.c
    compiler/lexer.c
    compiler/parser.c
    compiler/ast.c
    compiler/codegen.c
    compiler/mod_registry.c
    compiler/optimize.c
    compiler/driver.c
)
target_include_directories(forge PRIVATE include compiler)
target_sources(forge PRIVATE runtime/platform.c)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")

include(${CMAKE_SOURCE_DIR}/cmake/ForgeLibrary.cmake)

forge_add_library(greeting "${CMAKE_SOURCE_DIR}/libs/greeting/greeting.fg")
forge_add_library(mathutil "${CMAKE_SOURCE_DIR}/libs/mathutil/mathutil.fg")

file(GLOB FORGE_EXAMPLES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/examples/*.fg")

set(FORGE_EXAMPLE_TARGETS "")
foreach(example_file IN LISTS FORGE_EXAMPLES)
    get_filename_component(example_name "${example_file}" NAME_WE)
    if(example_name STREQUAL "use_library")
        continue()
    endif()
    if(example_name STREQUAL "web_server")
        continue()
    endif()
    set(gen_bin "${CMAKE_BINARY_DIR}/bin/${example_name}")

    add_custom_command(
        OUTPUT "${gen_bin}"
        COMMAND forge "${example_file}" -o "${gen_bin}"
            --forge-root "${CMAKE_SOURCE_DIR}" --lib-dir "${CMAKE_BINARY_DIR}/lib"
        DEPENDS forge forge_runtime "${example_file}"
        COMMENT "Compiling ${example_name}.fg (native)"
        VERBATIM
    )
    add_custom_target("${example_name}" DEPENDS "${gen_bin}")
    list(APPEND FORGE_EXAMPLE_TARGETS "${example_name}")
endforeach()

set(gen_use_lib "${CMAKE_BINARY_DIR}/bin/use_library")
add_custom_command(
    OUTPUT "${gen_use_lib}"
    COMMAND forge "${CMAKE_SOURCE_DIR}/examples/use_library.fg" -o "${gen_use_lib}"
        --forge-root "${CMAKE_SOURCE_DIR}" --lib-dir "${CMAKE_BINARY_DIR}/lib"
        -I "${CMAKE_BINARY_DIR}/generated/libs"
        -l forge_greeting -l forge_mathutil
    DEPENDS forge forge_runtime forge_lib_greeting forge_lib_mathutil "${CMAKE_SOURCE_DIR}/examples/use_library.fg"
    COMMENT "Compiling use_library.fg (native)"
    VERBATIM
)
add_custom_target(use_library DEPENDS "${gen_use_lib}")
list(APPEND FORGE_EXAMPLE_TARGETS use_library)

if(FORGE_EXAMPLE_TARGETS)
    add_custom_target(examples ALL DEPENDS ${FORGE_EXAMPLE_TARGETS})
endif()

# Self-hosted compiler bootstrap (stage0 C forge -> stage1 -> stage2)
set(FORGE_BOOTSTRAP_SRC "${CMAKE_SOURCE_DIR}/bootstrap/compiler.fg")
set(FORGE_STAGE1_BIN "${CMAKE_BINARY_DIR}/bin/forge-stage1")
set(FORGE_STAGE2_C "${CMAKE_BINARY_DIR}/generated/forge-stage2.c")
set(FORGE_STAGE2_BIN "${CMAKE_BINARY_DIR}/bin/forge-stage2")

add_custom_command(
    OUTPUT "${FORGE_STAGE1_BIN}"
    COMMAND forge "${FORGE_BOOTSTRAP_SRC}" -o "${FORGE_STAGE1_BIN}"
        --forge-root "${CMAKE_SOURCE_DIR}" --lib-dir "${CMAKE_BINARY_DIR}/lib"
    DEPENDS forge forge_runtime "${FORGE_BOOTSTRAP_SRC}"
    COMMENT "Bootstrap stage1: C forge compiles compiler.fg"
    VERBATIM
)

add_custom_target(forge-stage1 DEPENDS "${FORGE_STAGE1_BIN}")

add_custom_command(
    OUTPUT "${FORGE_STAGE2_BIN}"
    COMMAND "${FORGE_STAGE1_BIN}" "${FORGE_BOOTSTRAP_SRC}" -o "${FORGE_STAGE2_C}"
    COMMAND ${CMAKE_C_COMPILER} -std=c11 -O3
        "-I${CMAKE_SOURCE_DIR}/include"
        "${FORGE_STAGE2_C}"
        -o "${FORGE_STAGE2_BIN}"
        "-L${CMAKE_BINARY_DIR}/lib"
        -lforge_std -lforge_runtime -lm
    DEPENDS forge-stage1 "${FORGE_BOOTSTRAP_SRC}"
    COMMENT "Bootstrap stage2: self-hosted compiler compiles itself"
    VERBATIM
)

add_custom_target(forge-selfhost DEPENDS "${FORGE_STAGE2_BIN}")

# Self-hosted compiler compiles match.fg to C (bootstrap verification)
set(FORGE_SELFHOST_TEST_C "${CMAKE_BINARY_DIR}/generated/selfhost_match.c")
add_custom_command(
    OUTPUT "${FORGE_SELFHOST_TEST_C}"
    COMMAND "${FORGE_STAGE2_BIN}" "${CMAKE_SOURCE_DIR}/examples/match.fg"
        -o "${FORGE_SELFHOST_TEST_C}"
    DEPENDS forge-selfhost "${CMAKE_SOURCE_DIR}/examples/match.fg"
    COMMENT "Self-host test: stage2 compiles match.fg"
    VERBATIM
)
add_custom_target(forge-selfhost-test DEPENDS "${FORGE_SELFHOST_TEST_C}")

set(FORGE_SELFHOST_CONTROL_C "${CMAKE_BINARY_DIR}/generated/selfhost_control_flow.c")
add_custom_command(
    OUTPUT "${FORGE_SELFHOST_CONTROL_C}"
    COMMAND "${FORGE_STAGE2_BIN}" "${CMAKE_SOURCE_DIR}/examples/control_flow.fg"
        -o "${FORGE_SELFHOST_CONTROL_C}"
    DEPENDS forge-selfhost "${CMAKE_SOURCE_DIR}/examples/control_flow.fg"
    COMMENT "Self-host test: stage2 compiles control_flow.fg"
    VERBATIM
)
add_custom_target(forge-selfhost-control DEPENDS "${FORGE_SELFHOST_CONTROL_C}")

# Fixed-point check: stage2 output recompiles to identical stage3 output
set(FORGE_STAGE3_C "${CMAKE_BINARY_DIR}/generated/forge-stage3.c")
set(FORGE_STAGE3_BIN "${CMAKE_BINARY_DIR}/bin/forge-stage3")
set(FORGE_STAGE4_C "${CMAKE_BINARY_DIR}/generated/forge-stage4.c")
add_custom_command(
    OUTPUT "${FORGE_STAGE4_C}"
    COMMAND "${FORGE_STAGE2_BIN}" "${FORGE_BOOTSTRAP_SRC}" -o "${FORGE_STAGE3_C}"
    COMMAND ${CMAKE_C_COMPILER} -std=c11 -O3
        "-I${CMAKE_SOURCE_DIR}/include"
        "${FORGE_STAGE3_C}"
        -o "${FORGE_STAGE3_BIN}"
        "-L${CMAKE_BINARY_DIR}/lib"
        -lforge_std -lforge_runtime -lm
    COMMAND "${FORGE_STAGE3_BIN}" "${FORGE_BOOTSTRAP_SRC}" -o "${FORGE_STAGE4_C}"
    COMMAND ${CMAKE_COMMAND} -E compare_files "${FORGE_STAGE3_C}" "${FORGE_STAGE4_C}"
    DEPENDS forge-selfhost "${FORGE_BOOTSTRAP_SRC}"
    COMMENT "Self-host verify: stage3 recompiles compiler.fg to identical output"
    VERBATIM
)
add_custom_target(forge-selfhost-verify DEPENDS "${FORGE_STAGE4_C}")

# Benchmark HTTP server (Forge)
set(FORGE_BENCH_SRC "${CMAKE_SOURCE_DIR}/benchmark/forge/bench_server.fg")
set(FORGE_BENCH_BIN "${CMAKE_BINARY_DIR}/bin/bench_server")
add_custom_command(
    OUTPUT "${FORGE_BENCH_BIN}"
    COMMAND forge "${FORGE_BENCH_SRC}" -o "${FORGE_BENCH_BIN}"
        --forge-root "${CMAKE_SOURCE_DIR}" --lib-dir "${CMAKE_BINARY_DIR}/lib"
    DEPENDS forge forge_runtime "${FORGE_BENCH_SRC}"
    COMMENT "Compiling bench_server.fg (native)"
    VERBATIM
)
add_custom_target(bench_server DEPENDS "${FORGE_BENCH_BIN}")

# Simple web server example (native main, port 8080)
set(FORGE_WEB_SRC "${CMAKE_SOURCE_DIR}/examples/web_server.fg")
set(FORGE_WEB_BIN "${CMAKE_BINARY_DIR}/bin/web_server")
add_custom_command(
    OUTPUT "${FORGE_WEB_BIN}"
    COMMAND forge "${FORGE_WEB_SRC}" -o "${FORGE_WEB_BIN}"
        --forge-root "${CMAKE_SOURCE_DIR}" --lib-dir "${CMAKE_BINARY_DIR}/lib"
    DEPENDS forge forge_runtime "${FORGE_WEB_SRC}"
    COMMENT "Compiling web_server.fg (native)"
    VERBATIM
)
add_custom_target(web_server DEPENDS "${FORGE_WEB_BIN}")
