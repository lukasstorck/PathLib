cmake_minimum_required(VERSION 3.14)
project(PathLibTests CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()

include(FetchContent)

FetchContent_Declare(
  doctest
  GIT_REPOSITORY https://github.com/doctest/doctest
  GIT_TAG v2.4.12
)

FetchContent_MakeAvailable(doctest)

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)

# Automatically collect all test files
file(GLOB_RECURSE TEST_SOURCES "*.cpp")

add_executable(pathlib_tests ${TEST_SOURCES})

target_link_libraries(pathlib_tests PRIVATE PathLib::PathLib doctest::doctest)

doctest_discover_tests(pathlib_tests)
