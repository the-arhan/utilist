add_executable(Utilist
    src/main.cpp
    src/core/application_controller.cpp
    src/core/utility_manager.cpp
    src/core/utility_registry.cpp
    src/core/process_manager.cpp
    src/core/shortcut_manager.cpp
    src/core/configuration_manager.cpp
    src/core/ipc_client.cpp
    src/core/logging_service.cpp
    src/core/tray_controller.cpp
    src/platform/windows_paths.cpp
    src/platform/windows_process.cpp
    src/platform/windows_hotkeys.cpp
    src/platform/windows_startup.cpp
    src/ui/main_window.cpp
    src/ui/dashboard_page.cpp
    src/ui/utilities_page.cpp
    src/ui/shortcuts_page.cpp
    src/ui/settings_page.cpp
    src/ui/about_page.cpp
    src/ui/utility_card.cpp
)

set_target_properties(Utilist PROPERTIES
    AUTOMOC ON
    WIN32_EXECUTABLE FALSE # Keep console for development diagnostics, or TRUE for release
)

target_include_directories(Utilist
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(Utilist
    PRIVATE
        Qt6::Widgets
        utilist_shared
        kernel32
        user32
        shell32
        advapi32
        ole32
)

target_compile_features(Utilist PRIVATE cxx_std_20)

# Automatic Qt deployment for standalone execution
find_program(WINDEPLOYQT_EXE windeployqt HINTS "C:/Qt/6.8.0/msvc2022_64/bin" "${Qt6_DIR}/../../../bin")
if(WINDEPLOYQT_EXE)
    add_custom_command(TARGET Utilist POST_BUILD
        COMMAND "${WINDEPLOYQT_EXE}"
            --no-compiler-runtime
            "$<TARGET_FILE:Utilist>"
        COMMENT "Running windeployqt on Utilist..."
    )
endif()
