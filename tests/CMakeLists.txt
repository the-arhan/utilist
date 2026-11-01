add_executable(test_shared
    unit/test_shared.cpp
)

target_link_libraries(test_shared PRIVATE utilist_shared)
target_compile_features(test_shared PRIVATE cxx_std_20)

add_test(NAME SharedProtocolTests COMMAND test_shared)

add_executable(test_config
    unit/test_config.cpp
    ../app/src/core/configuration_manager.cpp
    ../app/src/platform/windows_hotkeys.cpp
)

target_include_directories(test_config PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/app/src
)

target_link_libraries(test_config PRIVATE utilist_shared)
target_compile_features(test_config PRIVATE cxx_std_20)

add_test(NAME ConfigurationTests COMMAND test_config)

add_executable(test_ipc_lifecycle
    integration/test_ipc_lifecycle.cpp
)

target_link_libraries(test_ipc_lifecycle PRIVATE utilist_shared)
target_compile_features(test_ipc_lifecycle PRIVATE cxx_std_20)

# Lifecycle tests for all utilities
add_dependencies(test_ipc_lifecycle HeartbeatUtility MTrail Awake InputShield WindowPin)
add_test(NAME LifecycleHeartbeat COMMAND test_ipc_lifecycle "$<TARGET_FILE:HeartbeatUtility>")
add_test(NAME LifecycleAwake COMMAND test_ipc_lifecycle "$<TARGET_FILE:Awake>")
add_test(NAME LifecycleInputShield COMMAND test_ipc_lifecycle "$<TARGET_FILE:InputShield>")
add_test(NAME LifecycleWindowPin COMMAND test_ipc_lifecycle "$<TARGET_FILE:WindowPin>")
add_test(NAME LifecycleMTrail COMMAND test_ipc_lifecycle "$<TARGET_FILE:MTrail>")
