ADD_EXECUTABLE ( pxinfo ptxinfo.cpp )

TARGET_LINK_LIBRARIES ( pxinfo
  Ptex_static
  ${PTHREAD_LIBRARY}
  ${Z_LIBRARY}
  )
