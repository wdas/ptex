cmake_minimum_required(VERSION 2.8)
project(ptex)

option(PTEX_BUILD_STATIC_LIBS "Enable building Ptex static libraries" ON)
option(PTEX_BUILD_SHARED_LIBS "Enable building Ptex shared libraries" ON)

include(GNUInstallDirs)
include(CTest)
include(FindZLIB)
include(FindThreads)

enable_testing()

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    # Detect the build type from the $FLAVOR environment variable
    # Default to optimized Release builds when unspecified.
    if ("$ENV{FLAVOR}" MATCHES "debug")
        set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "type of build" FORCE)
    elseif ("$ENV{FLAVOR}" MATCHES "profile")
        set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "type of build" FORCE)
    else ()
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING "type of build" FORCE)
    endif ()
endif ()

if (NOT WIN32)
    set(CMAKE_CXX_FLAGS "-std=c++98 -Wall -Wextra -pedantic ${CMAKE_CXX_FLAGS}")
endif ()

if(MSVC)
    add_definitions(

        #warning C4800: 'BOOL': forcing value to bool 'true' or 'false' (performance warning)
        /wd4800 

        #warning C4244: 'argument': conversion from 'double' to 'float', possible loss of data
        /wd4244 

        /D_CRT_SECURE_NO_WARNINGS
    )
endif()

if (${PRMAN_15_COMPATIBLE_PTEX})
    add_definitions(-DPTEX_NO_LARGE_METADATA_BLOCKS)
endif ()

include_directories(src/ptex)
include_directories(${ZLIB_INCLUDE_DIR})

add_subdirectory(src/ptex)
add_subdirectory(src/utils)
add_subdirectory(src/tests)
add_subdirectory(src/doc)
