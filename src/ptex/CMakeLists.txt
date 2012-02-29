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

ADD_LIBRARY ( Ptex_static STATIC 
  ${PTEX_SOURCE_FILES}
  )

SET_TARGET_PROPERTIES ( Ptex_static PROPERTIES OUTPUT_NAME Ptex )

ADD_LIBRARY ( Ptex SHARED
  ${PTEX_SOURCE_FILES}
  )
IF (APPLE)
  TARGET_LINK_LIBRARIES ( Ptex 
	${PTHREAD_LIBRARY}
	${Z_LIBRARY}
	)
ENDIF (APPLE)

INSTALL ( TARGETS Ptex_static DESTINATION ${CMAKE_INSTALL_PREFIX}/lib )
INSTALL ( TARGETS Ptex DESTINATION ${CMAKE_INSTALL_PREFIX}/lib )
INSTALL ( FILES 
  Ptexture.h 
  PtexHalf.h 
  PtexInt.h 
  PtexUtils.h
  DESTINATION
  ${CMAKE_INSTALL_PREFIX}/include
  )
