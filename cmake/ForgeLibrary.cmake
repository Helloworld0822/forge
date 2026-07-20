function(forge_add_library NAME SOURCE)
    set(gen_a "${CMAKE_BINARY_DIR}/lib/libforge_${NAME}.a")
    set(gen_h "${CMAKE_BINARY_DIR}/generated/libs/${NAME}.h")

    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated/libs")

    add_custom_command(
        OUTPUT "${gen_a}" "${gen_h}"
        COMMAND forge --lib "${SOURCE}" -o "${gen_a}" --header "${gen_h}"
            --forge-root "${CMAKE_SOURCE_DIR}" --lib-dir "${CMAKE_BINARY_DIR}/lib"
        DEPENDS forge forge_runtime "${SOURCE}"
        COMMENT "Compiling Forge library ${NAME}"
        VERBATIM
    )

    add_library(${NAME} STATIC IMPORTED GLOBAL)
    set_target_properties(${NAME} PROPERTIES
        IMPORTED_LOCATION "${gen_a}"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/generated/libs"
        INTERFACE_LINK_LIBRARIES forge_runtime
    )
endfunction()

function(forge_add_executable NAME SOURCE)
    set(gen_bin "${CMAKE_BINARY_DIR}/bin/${NAME}")

    add_custom_command(
        OUTPUT "${gen_bin}"
        COMMAND forge "${SOURCE}" -o "${gen_bin}"
            --forge-root "${CMAKE_SOURCE_DIR}" --lib-dir "${CMAKE_BINARY_DIR}/lib"
        DEPENDS forge forge_runtime "${SOURCE}"
        COMMENT "Compiling ${NAME}.fg to native binary"
        VERBATIM
    )

    add_custom_target(${NAME} DEPENDS "${gen_bin}")
endfunction()
