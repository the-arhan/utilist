add_executable(HeartbeatUtility
    src/main.cpp
)

target_link_libraries(HeartbeatUtility PRIVATE utilist_shared)
target_compile_features(HeartbeatUtility PRIVATE cxx_std_20)

# Copy manifest to output directory
add_custom_command(TARGET HeartbeatUtility POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/manifest.json"
        "$<TARGET_FILE_DIR:HeartbeatUtility>/manifest.json"
)
