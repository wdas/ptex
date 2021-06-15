@PACKAGE_INIT@

include("${CMAKE_CURRENT_LIST_DIR}/ptex-config-version.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ptex-exports.cmake")

set(CMAKE_THREAD_PREFER_PTHREAD ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_dependency(Threads REQUIRED)

# Provide PkgConfig::ZLIB to downstream dependents
find_dependency(PkgConfig REQUIRED)
pkg_checK_modules(Ptex_ZLIB REQUIRED zlib IMPORTED_TARGET)

set_and_check(Ptex_DIR @PACKAGE_CMAKE_INSTALL_PREFIX@)
set_and_check(Ptex_LIBRARY_DIRS @PACKAGE_CMAKE_INSTALL_LIBDIR@)
set_and_check(Ptex_INCLUDE_DIRS @PACKAGE_CMAKE_INSTALL_INCLUDEDIR@)

set(Ptex_FOUND TRUE)
