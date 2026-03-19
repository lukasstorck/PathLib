cmake_minimum_required(VERSION 3.14)

project(
  PathLib
  VERSION 1.0.0
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(PathLib INTERFACE)

add_library(PathLib::PathLib ALIAS PathLib)

target_include_directories(PathLib INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)

target_compile_definitions(PathLib INTERFACE _CRT_SECURE_NO_WARNINGS)
target_compile_features(PathLib INTERFACE cxx_std_23)

# only build tests when this repo is the top-level project
if(PROJECT_IS_TOP_LEVEL)
  enable_testing()
  add_subdirectory(tests)
endif()
