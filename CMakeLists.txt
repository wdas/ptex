cmake_minimum_required(VERSION 2.8)
project(ptex)

include(GNUInstallDirs)
include(CTest)
include(FindZLIB)
include(FindThreads)

enable_testing()

# Detect the build type from the $FLAVOR environment variable
if ("$ENV{FLAVOR}" MATCHES "debug")
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "type of build" FORCE)
else ()
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "type of build" FORCE)
endif ()

if (NOT WIN32)
    set(CMAKE_CXX_FLAGS "-std=c++98 -Wall -Wextra -pedantic")
endif ()

if (${PRMAN_15_COMPATIBLE_PTEX})
    add_definitions(-DPTEX_NO_LARGE_METADATA_BLOCKS)
endif ()

include_directories(src/ptex)
include_directories(${ZLIB_INCLUDE_DIR})

add_subdirectory(src/ptex)
add_subdirectory(src/utils)
add_subdirectory(src/tests)
add_subdirectory(src/doc)
