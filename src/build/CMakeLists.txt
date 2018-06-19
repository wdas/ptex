include(CMakePackageConfigHelpers)

set(CMAKE_DIR "share/cmake/Ptex")

write_basic_package_version_file("ptex-version.cmake"
    VERSION "${PTEX_MAJOR_VERSION}.${PTEX_MINOR_VERSION}"
    COMPATIBILITY AnyNewerVersion)

configure_package_config_file(
    "ptex-config.cmake" "ptex-config.cmake"
    INSTALL_DESTINATION "${CMAKE_DIR}" PATH_VARS
    CMAKE_INSTALL_PREFIX CMAKE_INSTALL_LIBDIR CMAKE_INSTALL_INCLUDEDIR)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ptex-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ptex-version.cmake"
    DESTINATION "${CMAKE_DIR}" COMPONENT "devel")

install(EXPORT Ptex NAMESPACE Ptex::
    FILE "ptex-exports.cmake" DESTINATION "${CMAKE_DIR}" COMPONENT "devel")
