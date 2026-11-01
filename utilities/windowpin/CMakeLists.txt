cmake_minimum_required(VERSION 3.20)
project(WindowPin VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT TARGET utilist_shared)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../shared/CMakeLists.txt")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../shared" "${CMAKE_CURRENT_BINARY_DIR}/utilist_shared")
    endif()
endif()

add_executable(WindowPin WIN32
    src/windowpin.cpp
)

target_link_libraries(WindowPin PRIVATE
    utilist_shared
    user32
    gdi32
    shell32
    dwmapi
)

target_compile_features(WindowPin PRIVATE cxx_std_20)

add_custom_command(TARGET WindowPin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/manifest.json"
    "$<TARGET_FILE_DIR:WindowPin>/manifest.json"
)

