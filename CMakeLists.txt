cmake_minimum_required(VERSION 3.8.0)
project(Ptex)

option(PTEX_BUILD_STATIC_LIBS "Enable building Ptex static libraries" ON)
option(PTEX_BUILD_SHARED_LIBS "Enable building Ptex shared libraries" ON)
option(PTEX_BUILD_DOCS "Enable building Ptex documentation (require Doxygen)" ON)
option(PRMAN_15_COMPATIBLE_PTEX "Enable PRMan 15 compatibility" OFF)

# The C++ standard can set either through the environment or by specifyign
# CMAKE_CXX_STANDARD when configuring the project using "cmake".
if (NOT DEFINED CMAKE_CXX_STANDARD)
    # export CXXFLAGS_STD=c++17 in the environment to enable C++17.
    if (DEFINED ENV{CXXFLAGS_STD})
        string(SUBSTRING "$ENV{CXXFLAGS_STD}" 3 -1 cxx_standard)
        set(CMAKE_CXX_STANDARD "${cxx_standard}")
    else ()
        set(CMAKE_CXX_STANDARD 98)
    endif ()
endif()
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_FIND_PACKAGE_RESOLVE_SYMLINKS ON)
set(CMAKE_INSTALL_MESSAGE LAZY)
set(CMAKE_THREAD_PREFER_PTHREAD ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)

include(GNUInstallDirs)

include(CTest)
enable_testing()

# Setup platform-specific threading flags.
find_package(Threads REQUIRED)

find_package(ZLIB REQUIRED)

if (NOT DEFINED PTEX_SHA)
    # Query git for current commit ID
    execute_process(
        COMMAND git rev-parse HEAD
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        OUTPUT_VARIABLE PTEX_SHA
        OUTPUT_STRIP_TRAILING_WHITESPACE)
endif ()

if (NOT DEFINED PTEX_VER)
    # Get the version string from a "version" file or from git
    if (EXISTS "${PROJECT_SOURCE_DIR}/version")
        file(STRINGS "${PROJECT_SOURCE_DIR}/version" PTEX_VER)
    else ()
        execute_process(
            COMMAND git describe --match=v[0-9]*.[0-9]* --first-parent HEAD
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            OUTPUT_VARIABLE PTEX_VER
            OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif ()
endif ()
# Provide a fallback version when the Git repository is unavailable.
if (NOT PTEX_VER)
    set(PTEX_VER "v2.4.X")
endif()

# Transform PTEX_VER into PTEX_MAJOR_VERSION and PTEX_MINOR_VERSION
string(REPLACE "v" "" PTEX_VER_STRIPPED ${PTEX_VER})  # strip leading "v"
string(REPLACE "." ";" PTEX_VER_LIST ${PTEX_VER_STRIPPED})
list(LENGTH PTEX_VER_LIST PTEX_VER_LENGTH)
if (${PTEX_VER_LENGTH} LESS 2)
    message(FATAL_ERROR "Could not determine the Ptex library version")
endif ()

# The version variables are used to generate PtexVersion.h
list(GET PTEX_VER_LIST 0 PTEX_MAJOR_VERSION)
list(GET PTEX_VER_LIST 1 PTEX_MINOR_VERSION)

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
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
endif ()

if (MSVC)
    add_definitions(
        # forcing value to bool 'true' or 'false' (performance warning)
        /wd4800
        # conversion from 'double' to 'float', possible loss of data
        /wd4244
        /D_CRT_SECURE_NO_WARNINGS
    )
endif()

# Set the default Ptex library to use for tools and tests.
if (PTEX_BUILD_STATIC_LIBS)
    set(PTEX_LIBRARY Ptex_static)
else()
    set(PTEX_LIBRARY Ptex_dynamic)
endif()

if (${PRMAN_15_COMPATIBLE_PTEX})
    add_definitions(-DPTEX_NO_LARGE_METADATA_BLOCKS)
endif ()

include_directories(src/ptex)

add_subdirectory(src/ptex)
add_subdirectory(src/utils)
add_subdirectory(src/tests)
if (PTEX_BUILD_DOCS)
    add_subdirectory(src/doc)
endif ()
add_subdirectory(src/build)
