add_executable(ptxinfo ptxinfo.cpp)
add_definitions(-DPTEX_VER="${PTEX_VER} \(${PTEX_SHA}\)")
if (PTEX_BUILD_STATIC_LIBS)
    add_definitions(-DPTEX_STATIC)
endif()

target_link_libraries(ptxinfo ${PTEX_LIBRARY} PkgConfig::Ptex_ZLIB)

install(TARGETS ptxinfo DESTINATION ${CMAKE_INSTALL_BINDIR})
