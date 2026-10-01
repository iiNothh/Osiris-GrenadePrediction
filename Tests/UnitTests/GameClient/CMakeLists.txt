add_subdirectory(EngineTrace)
add_subdirectory(GrenadePrediction)
add_subdirectory(Hud)
add_subdirectory(Panorama)

target_sources(UnitTests PRIVATE ConfigMaxCoordStateTests.cpp)
