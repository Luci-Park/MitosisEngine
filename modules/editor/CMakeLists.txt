engine_add_module(editor)

target_link_libraries(engine_editor
    PUBLIC mts::renderer
           mts::window
    PRIVATE mts::core
            mts::editortheme
            imgui::imgui
            iconfontcppheaders::iconfontcppheaders
)

target_sources(engine_editor PRIVATE src/editor/Editor.cpp)
