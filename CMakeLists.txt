cmake_minimum_required(VERSION 2.8.0)
project(flexetl)


set(CMAKE_CXX_FLAGS "-O3 -g -std=c++17 -m64")
add_definitions(-DNDEBUG)

#set(CMAKE_CXX_FLAGS "-g3 -std=c++17 -m64")



set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(CONTRIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/contrib")
include_directories(${CONTRIB_DIR}/libconfig/lib)
include_directories(${CONTRIB_DIR})

include_directories(./src/)

include(src/stem/project_version/get_version_git.cmake)
add_version_git(${CMAKE_BINARY_DIR})


add_subdirectory (src/stem/metrics)

add_subdirectory( contrib/libconfig)
add_subdirectory( contrib/gflags)
add_subdirectory( contrib/clickhouse-cpp clickhouse-cpp)
add_subdirectory( contrib/cityhash cityhash)
add_subdirectory( contrib/lz4 lz4)

add_subdirectory( src/flexetl flexetl)
#add_subdirectory( src/jsontest jsontest)
