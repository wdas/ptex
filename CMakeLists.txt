cmake_minimum_required(VERSION 2.8)
project(ptex)

include(GNUInstallDirs)
include(CTest)
include(FindZLIB)
include(FindThreads)

enable_testing()

include_directories(src/ptex)
include_directories(${ZLIB_INCLUDE_DIR})

add_subdirectory(src/ptex)
add_subdirectory(src/utils)
add_subdirectory(src/tests)
add_subdirectory(src/doc)
