cmake_minimum_required(VERSION 3.20)
project(Awake VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT TARGET utilist_shared)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../shared/CMakeLists.txt")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../shared" "${CMAKE_CURRENT_BINARY_DIR}/utilist_shared")
    endif()
endif()

add_executable(Awake WIN32
    src/awake.cpp
)

target_link_libraries(Awake PRIVATE
    utilist_shared
    shell32
    user32
)

target_compile_features(Awake PRIVATE cxx_std_20)

add_custom_command(TARGET Awake POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/manifest.json"
    "$<TARGET_FILE_DIR:Awake>/manifest.json"
)

