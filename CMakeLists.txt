cmake_minimum_required(VERSION 3.7)
project(VkEngine)

# CMake 3.7 added the FindVulkan module 
message(STATUS "Finding Vulkan SDK using CMake...")
find_package(Vulkan)
message(STATUS "Vulkan FOUND = ${Vulkan_FOUND}")
message(STATUS "Vulkan Include = ${Vulkan_INCLUDE_DIR}")
message(STATUS "Vulkan Lib = ${Vulkan_LIBRARY}")
if(NOT Vulkan_FOUND)
    message("Error: Couldn't Locate the VulkanSDK!")
    return()
endif()


# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

add_subdirectory(external/glfw)
target_link_libraries(VkEngine glfw)

include_directories(
    "external/include"
)

add_subdirectory(renderer)
add_subdirectory(application)
add_subdirectory(vendor)