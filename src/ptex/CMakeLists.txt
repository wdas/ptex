if (WIN32)
    add_definitions(/DPTEX_EXPORTS)
endif (WIN32)

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

add_library(Ptex_static STATIC ${SRCS})
set_target_properties(Ptex_static PROPERTIES OUTPUT_NAME Ptex)

add_library(Ptex_dynamic SHARED ${SRCS})
set_target_properties(Ptex_dynamic PROPERTIES OUTPUT_NAME Ptex)

target_link_libraries(Ptex_dynamic ${CMAKE_THREAD_LIBS_INIT} ${ZLIB_LIBRARIES})

install(TARGETS Ptex_static Ptex_dynamic DESTINATION ${CMAKE_INSTALL_LIBDIR})

install(FILES
        PtexHalf.h
        PtexInt.h
        Ptexture.h
        PtexUtils.h
        PtexVersion.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
