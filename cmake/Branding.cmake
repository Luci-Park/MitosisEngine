function(engine_copy_branding target)
    cmake_parse_arguments(ARG "" "SOURCE_DIR" "" ${ARGN})

    if(NOT ARG_SOURCE_DIR)
        message(FATAL_ERROR "engine_copy_branding: SOURCE_DIR is required")
    endif()

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "engine_copy_branding: unrecognised arguments (keywords are "
            "case-sensitive): ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    get_filename_component(source_abs "${ARG_SOURCE_DIR}" ABSOLUTE)

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory
                "$<TARGET_FILE_DIR:${target}>/branding"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
                "${source_abs}" "$<TARGET_FILE_DIR:${target}>/branding"
        COMMENT "Copying branding assets next to ${target}"
        VERBATIM)
endfunction()
