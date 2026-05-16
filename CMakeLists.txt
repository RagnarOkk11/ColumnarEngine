cmake_minimum_required(VERSION 3.30)

project(columnar-engine)

option(ENABLE_SEMINARS OFF)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(cmake/BuildFlags.cmake)
include(cmake/TestSolution.cmake)
include(cmake/SeminarUtils.cmake)

include(FetchContent)

set(ABSL_PROPAGATE_CXX_STD ON)
set(BUILD_TESTING OFF)
set(ABSL_ENABLE_INSTALL ON)

FetchContent_Declare(
        abseil-cpp
        GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
        GIT_TAG 20240116.2
        SYSTEM
)
FetchContent_MakeAvailable(abseil-cpp)

FetchContent_Declare(
        re2
        GIT_REPOSITORY https://github.com/google/re2.git
        GIT_TAG 2024-02-01
        SYSTEM
)
FetchContent_MakeAvailable(re2)

find_package(Catch REQUIRED)
find_package(Benchmark REQUIRED)
find_package(GTest REQUIRED)

include_directories(util)

add_subdirectory(allocations_checker)

add_subdirectory(src)
