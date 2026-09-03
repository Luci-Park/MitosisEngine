engine_add_module(editortheme)

target_link_libraries(engine_editortheme PUBLIC imgui::imgui)

target_sources(engine_editortheme PRIVATE src/editortheme/EditorTheme.cpp)
