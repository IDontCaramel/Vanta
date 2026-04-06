cmake_minimum_required(VERSION 3.14)
project(vanta)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VANTA_CORE_SOURCES
    src/cli.cpp
    src/debug_output.cpp
    src/diagnostic.cpp
    src/lexer.cpp
    src/parser.cpp
    src/source_file.cpp
    src/value.cpp
    src/environment.cpp
    src/evaluator.cpp
)

set(VANTA_TEST_SOURCES
    tests/test_cli.cpp
    tests/test_lexer.cpp
    tests/test_parser.cpp
    tests/test_evaluator.cpp
    tests/test_runtime.cpp
    tests/test_source_file.cpp
    tests/test_integration.cpp
)

add_library(vanta_core ${VANTA_CORE_SOURCES})
target_include_directories(vanta_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Main executable
add_executable(vanta src/main.cpp)
target_link_libraries(vanta PRIVATE vanta_core)

# Testing
include(CTest)

if(BUILD_TESTING)
    option(VANTA_FETCH_GTEST "Download GoogleTest when it is not already installed" ON)

    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        if(NOT VANTA_FETCH_GTEST)
            message(FATAL_ERROR "GoogleTest was not found. Install GTest or reconfigure with -DVANTA_FETCH_GTEST=ON.")
        endif()

        include(FetchContent)
        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    if(TARGET GTest::gtest)
        set(VANTA_GTEST_TARGET GTest::gtest)
        set(VANTA_GTEST_MAIN_TARGET GTest::gtest_main)
    else()
        set(VANTA_GTEST_TARGET GTest::GTest)
        set(VANTA_GTEST_MAIN_TARGET GTest::Main)
    endif()

    add_executable(vanta_tests ${VANTA_TEST_SOURCES})
    target_link_libraries(vanta_tests PRIVATE vanta_core ${VANTA_GTEST_TARGET} ${VANTA_GTEST_MAIN_TARGET})
    add_dependencies(vanta_tests vanta)
    add_test(NAME VantaTests COMMAND vanta_tests)
endif()
