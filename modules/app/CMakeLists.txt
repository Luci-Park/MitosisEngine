engine_add_module(app)

target_link_libraries(engine_app
    PUBLIC mir::assets
           mir::core
           mir::window
           mir::renderer
           mir::editor
           mir::scene
           mir::script
    PRIVATE mir::input
)

target_sources(engine_app PRIVATE src/app/App.cpp)
