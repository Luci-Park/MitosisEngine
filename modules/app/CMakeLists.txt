engine_add_module(app)

target_link_libraries(engine_app
    PUBLIC mts::assets
           mts::core
           mts::window
           mts::renderer
    PRIVATE imgui::imgui
            mts::editortheme
            iconfontcppheaders::iconfontcppheaders
)

target_sources(engine_app PRIVATE src/app/App.cpp)
