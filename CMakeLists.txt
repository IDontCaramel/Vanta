cmake_minimum_required(VERSION 3.10)
project(vanta)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VANTA_SOURCES
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/source_file.cpp
    src/value.cpp
    src/environment.cpp
    src/evaluator.cpp
)

set(VANTA_TEST_SOURCES
    tests/test_lexer.cpp
    tests/test_parser.cpp
    tests/test_source_file.cpp
    tests/test_runtime.cpp
    tests/test_integration.cpp
)

# Main executable
add_executable(vanta ${VANTA_SOURCES})
target_include_directories(vanta PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Testing
enable_testing()
find_package(GTest REQUIRED)
add_executable(vanta_tests ${VANTA_TEST_SOURCES} src/lexer.cpp src/parser.cpp src/source_file.cpp src/value.cpp src/environment.cpp src/evaluator.cpp)
target_include_directories(vanta_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(vanta_tests GTest::GTest GTest::Main)
add_test(NAME VantaTests COMMAND vanta_tests)
