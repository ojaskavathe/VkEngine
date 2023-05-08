add_executable(VkEngine Application.cpp)

file(COPY ${CMAKE_SOURCE_DIR}/res DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE})

target_link_libraries(VkEngine PRIVATE renderer)
target_include_directories(VkEngine PRIVATE ${CMAKE_SOURCE_DIR}/renderer/includes)