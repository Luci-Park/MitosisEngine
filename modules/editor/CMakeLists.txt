engine_add_module(editor)

target_link_libraries(engine_editor
    PUBLIC mir::renderer
           mir::window
    PRIVATE mir::core
            mir::editortheme
            imgui::imgui
            iconfontcppheaders::iconfontcppheaders
)

target_sources(engine_editor PRIVATE src/editor/Editor.cpp)
