cmake_minimum_required(VERSION 2.8)
project(ptex)

# enable testing for the project
include(CTest)
enable_testing()

include(FindZLIB)
include(FindThreads)

include_directories(ptex)
include_directories(${ZLIB_INCLUDE_DIR})

add_subdirectory(ptex)
add_subdirectory(utils)
add_subdirectory(tests)
add_subdirectory(doc)
