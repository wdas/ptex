ADD_EXECUTABLE ( pxinfo ptxinfo.cpp )

TARGET_LINK_LIBRARIES ( pxinfo
  Ptex
  ${PTHREAD_LIBRARY}
  ${Z_LIBRARY}
  )
