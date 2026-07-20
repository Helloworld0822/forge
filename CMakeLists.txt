cmake_minimum_required(VERSION 3.16)
project(hylo VERSION 0.3.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

add_compile_options(-Wall -Wextra)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

add_library(hylo_std STATIC
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
)
target_include_directories(hylo_std PUBLIC include)

add_library(hylo_runtime STATIC
    runtime/runtime.c
    runtime/scheduler.c
)
target_link_libraries(hylo_runtime PUBLIC hylo_std)
target_include_directories(hylo_runtime PUBLIC include)

add_executable(hylo
    compiler/main.c
    compiler/lexer.c
    compiler/parser.c
    compiler/ast.c
    compiler/codegen.c
    compiler/mod_registry.c
)
target_include_directories(hylo PRIVATE include compiler)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")

include(${CMAKE_SOURCE_DIR}/cmake/HyloLibrary.cmake)

hylo_add_library(greeting "${CMAKE_SOURCE_DIR}/libs/greeting/greeting.hy")
hylo_add_library(mathutil "${CMAKE_SOURCE_DIR}/libs/mathutil/mathutil.hy")

file(GLOB HYLO_EXAMPLES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/examples/*.hy")

set(HYLO_EXAMPLE_TARGETS "")
foreach(example_file IN LISTS HYLO_EXAMPLES)
    get_filename_component(example_name "${example_file}" NAME_WE)
    if(example_name STREQUAL "use_library")
        continue()
    endif()
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

set(gen_use_lib "${CMAKE_BINARY_DIR}/generated/use_library.c")
add_custom_command(
    OUTPUT "${gen_use_lib}"
    COMMAND hylo "${CMAKE_SOURCE_DIR}/examples/use_library.hy" -o "${gen_use_lib}"
    DEPENDS hylo greeting mathutil "${CMAKE_SOURCE_DIR}/examples/use_library.hy"
    COMMENT "Compiling use_library.hy"
    VERBATIM
)
add_executable(use_library "${gen_use_lib}")
target_link_libraries(use_library PRIVATE hylo_runtime greeting mathutil)
target_include_directories(use_library PRIVATE "${CMAKE_BINARY_DIR}/generated/libs")
list(APPEND HYLO_EXAMPLE_TARGETS use_library)

if(HYLO_EXAMPLE_TARGETS)
    add_custom_target(examples ALL DEPENDS ${HYLO_EXAMPLE_TARGETS})
endif()
