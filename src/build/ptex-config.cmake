@PACKAGE_INIT@

include("${CMAKE_CURRENT_LIST_DIR}/ptex-version.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ptex-exports.cmake")

set_and_check(Ptex_DIR @PACKAGE_CMAKE_INSTALL_PREFIX@)
set_and_check(Ptex_LIBRARY_DIRS @PACKAGE_CMAKE_INSTALL_LIBDIR@)
set_and_check(Ptex_INCLUDE_DIRS @PACKAGE_CMAKE_INSTALL_INCLUDEDIR@)

set(Ptex_FOUND TRUE)
