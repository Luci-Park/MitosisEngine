
function(engine_add_module name)
    # set a target with name engine_${name}
    set(target engine_${name})

    add_library(${target})
    add_library(mts::${name} ALIAS ${target})
    target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
    
endfunction()