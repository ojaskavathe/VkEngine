add_library(
    renderer
    Engine.hpp
    Engine.cpp
)

target_include_directories(renderer PUBLIC "{CMAKE_CURRENT_SOURCE_DIR}")