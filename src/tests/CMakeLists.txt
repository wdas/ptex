ADD_EXECUTABLE ( halftest
  halftest.cpp)
TARGET_LINK_LIBRARIES ( halftest
  Ptex
  ${PTHREAD_LIBRARY}
  ${Z_LIBRARY}
 )

ADD_EXECUTABLE ( wtest
  wtest.cpp)
TARGET_LINK_LIBRARIES ( wtest
  Ptex
  ${PTHREAD_LIBRARY}
  ${Z_LIBRARY}
  )

ADD_EXECUTABLE ( rtest
  rtest.cpp)
TARGET_LINK_LIBRARIES ( rtest
 Ptex
  ${PTHREAD_LIBRARY}
  ${Z_LIBRARY}
 )

ADD_EXECUTABLE ( ftest
  ftest.cpp)
TARGET_LINK_LIBRARIES ( ftest
  Ptex
  ${PTHREAD_LIBRARY}
  ${Z_LIBRARY}
 )

ADD_TEST ( HalfFloatTest halftest )
ADD_TEST ( WriteTest wtest )
ADD_TEST ( ReadTest rtest )
