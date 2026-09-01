engine_add_module(assets)

target_sources(engine_assets PRIVATE
    src/assets/AssetBlob.cpp
    src/assets/AssetId.cpp
    src/assets/AssetManifest.cpp
)

target_link_libraries(engine_assets PUBLIC mts::core)
