cmake_minimum_required(VERSION 3.20)
project(MTrail VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT TARGET utilist_shared)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../shared/CMakeLists.txt")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../shared" "${CMAKE_CURRENT_BINARY_DIR}/utilist_shared")
    endif()
endif()

add_executable(MTrail WIN32 src/TrailOverlay_Optimized.cpp)

target_link_libraries(MTrail PRIVATE 
    d2d1
    dcomp
    d3d11
    dxgi
    shcore
    utilist_shared
)

add_custom_command(TARGET MTrail POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/manifest.json"
    "$<TARGET_FILE_DIR:MTrail>/manifest.json"
)