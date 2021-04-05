configure_file(PtexVersion.h.in
    ${CMAKE_CURRENT_SOURCE_DIR}/PtexVersion.h @ONLY)

set(SRCS
    PtexCache.cpp
    PtexFilters.cpp
    PtexHalf.cpp
    PtexReader.cpp
    PtexSeparableFilter.cpp
    PtexSeparableKernel.cpp
    PtexTriangleFilter.cpp
    PtexTriangleKernel.cpp
    PtexUtils.cpp
    PtexWriter.cpp)

if(PTEX_BUILD_STATIC_LIBS)
    add_library(Ptex_static STATIC ${SRCS})
    set_target_properties(Ptex_static PROPERTIES OUTPUT_NAME Ptex)
    target_include_directories(Ptex_static
    PUBLIC
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(Ptex_static
        PUBLIC Threads::Threads PkgConfig::Ptex_ZLIB)
    install(TARGETS Ptex_static EXPORT Ptex DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

if(PTEX_BUILD_SHARED_LIBS)
    add_library(Ptex_dynamic SHARED ${SRCS})
    set_target_properties(Ptex_dynamic PROPERTIES
        OUTPUT_NAME Ptex
        SOVERSION "${PTEX_MAJOR_VERSION}.${PTEX_MINOR_VERSION}")
    target_include_directories(Ptex_dynamic
        PUBLIC
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR})
    target_compile_definitions(Ptex_dynamic PRIVATE PTEX_EXPORTS)
    target_link_libraries(Ptex_dynamic
        PUBLIC Threads::Threads PkgConfig::Ptex_ZLIB)
    install(TARGETS Ptex_dynamic EXPORT Ptex DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()

install(FILES
        PtexExports.h
        PtexHalf.h
        PtexInt.h
        Ptexture.h
        PtexUtils.h
        PtexVersion.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT devel)
