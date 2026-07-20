cmake_minimum_required(VERSION 3.16)
project(hylo VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-Wall -Wextra)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_library(hylo_runtime STATIC
    runtime/runtime.c
    runtime/scheduler.c
)
target_include_directories(hylo_runtime PUBLIC include)

add_executable(hylo
    compiler/main.c
    compiler/lexer.c
    compiler/parser.c
    compiler/ast.c
    compiler/codegen.c
)
target_include_directories(hylo PRIVATE include compiler)

file(GLOB HYLO_EXAMPLES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/examples/*.hy")

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")

set(HYLO_EXAMPLE_TARGETS "")
foreach(example_file IN LISTS HYLO_EXAMPLES)
    get_filename_component(example_name "${example_file}" NAME_WE)
    set(generated_c "${CMAKE_BINARY_DIR}/generated/${example_name}.c")

    add_custom_command(
        OUTPUT "${generated_c}"
        COMMAND hylo "${example_file}" -o "${generated_c}"
        DEPENDS hylo "${example_file}"
        COMMENT "Compiling ${example_name}.hy"
        VERBATIM
    )

    add_executable("${example_name}" "${generated_c}")
    target_link_libraries("${example_name}" PRIVATE hylo_runtime)
    list(APPEND HYLO_EXAMPLE_TARGETS "${example_name}")
endforeach()

if(HYLO_EXAMPLE_TARGETS)
    add_custom_target(examples ALL DEPENDS ${HYLO_EXAMPLE_TARGETS})
endif()
