cmake_minimum_required(VERSION 3.10)
project(vanta)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Main executable
add_executable(vanta src/main.cpp src/lexer.cpp)
target_include_directories(vanta PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Testing
enable_testing()
find_package(GTest REQUIRED)
add_executable(vanta_tests tests/test_lexer.cpp src/lexer.cpp)
target_include_directories(vanta_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(vanta_tests GTest::GTest GTest::Main)
add_test(NAME VantaTests COMMAND vanta_tests)
