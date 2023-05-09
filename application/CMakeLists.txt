project(application)
add_executable(application Application.cpp)

add_custom_command(
  TARGET application POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/res
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/res
  COMMENT "Copying res to output directory: ${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}")

# file(COPY ${CMAKE_SOURCE_DIR}/res DESTINATION ${RUNTIME_OUTPUT_DIRECTORY})

target_link_libraries(application PRIVATE renderer)
target_include_directories(application PRIVATE ${CMAKE_SOURCE_DIR}/renderer/includes)