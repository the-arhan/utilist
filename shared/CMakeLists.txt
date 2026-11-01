add_library(utilist_shared STATIC
    src/protocol/json_helper.cpp
    src/protocol/pipe_channel.cpp
    src/models/utility_descriptor.cpp
)

target_include_directories(utilist_shared
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(utilist_shared PUBLIC cxx_std_20)

if(WIN32)
    target_link_libraries(utilist_shared PUBLIC kernel32 advapi32)
endif()
