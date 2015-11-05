cmake_minimum_required(VERSION 2.8)
project(ptex)

include(GNUInstallDirs)
include(CTest)
include(FindZLIB)
include(FindThreads)

enable_testing()

include_directories(ptex)
include_directories(${ZLIB_INCLUDE_DIR})

add_subdirectory(ptex)
add_subdirectory(utils)
add_subdirectory(tests)
add_subdirectory(doc)
