
function(engine_add_module name)
    # set a target with name engine_${name}
    set(target engine_${name})

    add_library(${target})
    add_library(mts::${name} ALIAS ${target})
    target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

endfunction()

# Registers a CTest executable for a module
# Sources are relative to modules/<name>/tests/
function(engine_add_module_tests name)
    if(NOT BUILD_TESTING)
        return()
    endif()

    set(target engine_${name}_tests)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE mts::${name} Catch2::Catch2WithMain)

    # One CTest case per Catch2 TEST_CASE, so failures show up individually, not as one blob.
    catch_discover_tests(${target})
endfunction()