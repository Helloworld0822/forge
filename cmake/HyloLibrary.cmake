function(hylo_add_library NAME SOURCE)
    set(gen_c "${CMAKE_BINARY_DIR}/generated/libs/${NAME}.c")
    set(gen_h "${CMAKE_BINARY_DIR}/generated/libs/${NAME}.h")

    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated/libs")

    add_custom_command(
        OUTPUT "${gen_c}" "${gen_h}"
        COMMAND hylo --lib "${SOURCE}" -o "${gen_c}" --header "${gen_h}"
        DEPENDS hylo "${SOURCE}"
        COMMENT "Compiling Hylo library ${NAME}"
        VERBATIM
    )

    add_library(${NAME} STATIC "${gen_c}")
    target_link_libraries(${NAME} PUBLIC hylo_runtime)
    target_include_directories(${NAME} PUBLIC "${CMAKE_BINARY_DIR}/generated/libs")
    set_target_properties(${NAME} PROPERTIES OUTPUT_NAME "hylo_${NAME}")
endfunction()

function(hylo_add_executable NAME SOURCE)
    set(gen_c "${CMAKE_BINARY_DIR}/generated/${NAME}.c")

    add_custom_command(
        OUTPUT "${gen_c}"
        COMMAND hylo "${SOURCE}" -o "${gen_c}"
        DEPENDS hylo "${SOURCE}"
        COMMENT "Compiling ${NAME}.hy"
        VERBATIM
    )

    add_executable(${NAME} "${gen_c}")
    target_link_libraries(${NAME} PRIVATE hylo_runtime)
    target_include_directories(${NAME} PRIVATE "${CMAKE_BINARY_DIR}/generated/libs")
endfunction()
