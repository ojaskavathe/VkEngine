project(renderer)

add_library(
    renderer STATIC
    Engine.cpp includes/Engine.hpp
)

# CMake 3.7 added the FindVulkan module 
message(STATUS "Finding Vulkan SDK using CMake...")
find_package(Vulkan)
message(STATUS "Vulkan FOUND = ${Vulkan_FOUND}")
message(STATUS "Vulkan Include = ${Vulkan_INCLUDE_DIR}")
message(STATUS "Vulkan Lib = ${Vulkan_LIBRARY}")
if(NOT Vulkan_FOUND)
    message("Error: Couldn't Locate the Vulkan SDK!")
    return()
endif()

# Vulkan, GLFW & GLM
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_SOURCE_DIR}/external/glfw ${CMAKE_BINARY_DIR}/external/glfw)
add_subdirectory(${CMAKE_SOURCE_DIR}/external/glm ${CMAKE_BINARY_DIR}/external/glm)
target_link_libraries(renderer
    PRIVATE ${Vulkan_LIBRARIES}
    PRIVATE glfw
    PRIVATE glm
)

target_include_directories(renderer 
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/utils
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/includes
    PUBLIC ${Vulkan_INCLUDE_DIR}
    PUBLIC ${CMAKE_SOURCE_DIR}/external/glfw/include
)

