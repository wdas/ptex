IF ( WIN32 )
  ADD_DEFINITIONS ( /DPTEX_EXPORTS )
ENDIF ( WIN32 )

SET ( PTEX_SOURCE_FILES 
  PtexCache.cpp 
  PtexFilters.cpp 
  PtexHalf.cpp 
  PtexReader.cpp 
  PtexSeparableFilter.cpp 
  PtexSeparableKernel.cpp 
  PtexTriangleFilter.cpp 
  PtexTriangleKernel.cpp 
  PtexUtils.cpp 
  PtexWriter.cpp
  )

IF ( BUILD_STATIC )
  ADD_LIBRARY ( libPtex STATIC 
    ${PTEX_SOURCE_FILES}
    )
ENDIF ( BUILD_STATIC )

ADD_LIBRARY ( Ptex SHARED
  ${PTEX_SOURCE_FILES}
  )
# IF (APPLE)
  TARGET_LINK_LIBRARIES ( Ptex 
	${PTHREAD_LIBRARY}
	${Z_LIBRARY}
	)
# ENDIF (APPLE)

IF ( BUILD_STATIC )
  INSTALL ( TARGETS libPtex DESTINATION ${CMAKE_INSTALL_PREFIX}/lib )
ENDIF ( BUILD_STATIC )

INSTALL ( TARGETS Ptex DESTINATION ${CMAKE_INSTALL_PREFIX}/lib )
INSTALL ( FILES 
  Ptexture.h 
  PtexHalf.h 
  PtexInt.h 
  PtexUtils.h
  DESTINATION
  ${CMAKE_INSTALL_PREFIX}/include
  )
