# engine_cook_assets(<target>
#     SOURCE_ROOTS <dir> [<dir> ...]
#     OUT_DIR <dir>)
#
# Runs AssetCooker over every SOURCE_ROOTS directory into OUT_DIR, then copies
# OUT_DIR next to the executable as cooked/ so the run directory is
# self-contained and the exe works when moved.
function(engine_cook_assets target)
    cmake_parse_arguments(ARG "" "OUT_DIR" "SOURCE_ROOTS" ${ARGN})

    if(NOT ARG_SOURCE_ROOTS)
        message(FATAL_ERROR "engine_cook_assets: SOURCE_ROOTS is required")
    endif()

    if(NOT ARG_OUT_DIR)
        message(FATAL_ERROR "engine_cook_assets: OUT_DIR is required")
    endif()

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "engine_cook_assets: unrecognised arguments (keywords are "
            "case-sensitive): ${ARG_UNPARSED_ARGUMENTS}")
    endif()

    set(source_files "")
    set(cooker_args "")

    foreach(root IN LISTS ARG_SOURCE_ROOTS)
        get_filename_component(root_abs "${root}" ABSOLUTE)

        # CONFIGURE_DEPENDS: a newly added asset file needs a reconfigure to
        # join this list, same reasoning as vcpkg.json in the root CMakeLists.
        file(GLOB_RECURSE root_files CONFIGURE_DEPENDS "${root_abs}/*")
        list(APPEND source_files ${root_files})

        # Passed as given, not resolved to absolute: AssetCooker hashes
        # "<root>/<relative path>" into each asset's id, so an absolute path
        # here would bake this machine's checkout location into every id.
        list(APPEND cooker_args --source "${root}")
    endforeach()

    set(out_dir "${ARG_OUT_DIR}")

    # AssetCooker decides output filenames itself (hashed asset ids), so
    # unlike Shaders.cmake the exact OUTPUT set isn't known at configure time.
    # A stamp file stands in for it; DEPENDS on every source file still gives
    # recook-on-edit, and DEPENDS on the AssetCooker target recooks when the
    # cooker itself changes.
    set(stamp "${CMAKE_BINARY_DIR}/${target}_assets_cooked.stamp")

    add_custom_command(
        OUTPUT "${stamp}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${out_dir}"
        COMMAND AssetCooker ${cooker_args} --out "${out_dir}"
        COMMAND "${CMAKE_COMMAND}" -E touch "${stamp}"
        DEPENDS AssetCooker ${source_files}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Cooking assets for ${target}"
        VERBATIM
        COMMAND_EXPAND_LISTS)

    set(cook_target ${target}_cook_assets)
    add_custom_target(${cook_target} DEPENDS "${stamp}")
    add_dependencies(${target} ${cook_target})

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory
                "$<TARGET_FILE_DIR:${target}>/cooked"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${out_dir}" "$<TARGET_FILE_DIR:${target}>/cooked"
        COMMENT "Copying cooked assets next to ${target}"
        VERBATIM
        COMMAND_EXPAND_LISTS)
endfunction()
