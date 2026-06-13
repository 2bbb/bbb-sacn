cmake_minimum_required(VERSION 3.19)
project(bbb-sacn VERSION 0.1.0 LANGUAGES CXX)

add_library(bbb_sacn INTERFACE)
add_library(bbb::sacn ALIAS bbb_sacn)

target_compile_features(bbb_sacn INTERFACE cxx_std_17)
target_include_directories(bbb_sacn INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

option(BBB_SACN_BUILD_TESTS "Build bbb-sacn tests" OFF)

if(BBB_SACN_BUILD_TESTS)
    enable_testing()

    add_executable(bbb_sacn_protocol_tests
        tests/protocol_tests.cpp
    )
    target_link_libraries(bbb_sacn_protocol_tests PRIVATE bbb::sacn)
    if(WIN32)
        target_link_libraries(bbb_sacn_protocol_tests PRIVATE ws2_32 iphlpapi)
    endif()
    add_test(NAME bbb_sacn_protocol_tests COMMAND bbb_sacn_protocol_tests)
endif()
