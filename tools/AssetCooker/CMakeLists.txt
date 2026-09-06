add_executable(AssetCooker)
target_sources(AssetCooker PRIVATE main.cpp)
target_link_libraries(AssetCooker PRIVATE mir::assets mir::core)
