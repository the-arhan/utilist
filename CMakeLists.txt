cmake_minimum_required(VERSION 3.20)

project(Utilist
    VERSION 1.0.0
    DESCRIPTION "Extensible Windows Desktop Utility Platform"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for language servers
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# MSVC specific options
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
endif()

# Build options
option(UTILIST_BUILD_APP "Build the UTILIST Qt 6 GUI application" ON)
option(UTILIST_BUILD_TESTS "Build automated unit and integration tests" ON)
option(UTILIST_BUILD_EXAMPLES "Build example utilities" ON)

# Shared protocol and models library
add_subdirectory(shared)

# Example utilities
if(UTILIST_BUILD_EXAMPLES)
    add_subdirectory(utilities/examples/heartbeat-utility)
endif()

# Bundled utilities
option(UTILIST_BUILD_UTILITIES "Build bundled utilities" ON)
if(UTILIST_BUILD_UTILITIES)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/utilities/mtrail/CMakeLists.txt")
        add_subdirectory(utilities/mtrail)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/utilities/awake/CMakeLists.txt")
        add_subdirectory(utilities/awake)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/utilities/inputshield/CMakeLists.txt")
        add_subdirectory(utilities/inputshield)
    endif()
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/utilities/windowpin/CMakeLists.txt")
        add_subdirectory(utilities/windowpin)
    endif()
endif()

# Automated tests
if(UTILIST_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Main GUI application
if(UTILIST_BUILD_APP)
    find_package(Qt6 COMPONENTS Widgets QUIET)
    if(Qt6_FOUND)
        message(STATUS "Qt 6 Widgets found (${Qt6_VERSION}). Configuring UTILIST GUI application.")
        add_subdirectory(app)
    else()
        message(WARNING "Qt 6 Widgets was not found. UTILIST GUI application will be skipped. "
                        "To build the GUI, install Qt 6 (MSVC 2022 x64) and set CMAKE_PREFIX_PATH.")
    endif()
endif()
