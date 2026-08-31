target_sources(UnitTests PRIVATE
  EngineTraceTests.cpp
  EngineTraceTypesTests.cpp
)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_sources(UnitTests PRIVATE
    EngineTraceProfileTests.cpp
  )
endif()
