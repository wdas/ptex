include(CMakePackageConfigHelpers)

set(CMAKE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/Ptex")

write_basic_package_version_file("ptex-config-version.cmake"
    VERSION "${PTEX_MAJOR_VERSION}.${PTEX_MINOR_VERSION}"
    COMPATIBILITY AnyNewerVersion)

configure_package_config_file(
    "ptex-config.cmake" "ptex-config.cmake"
    INSTALL_DESTINATION "${CMAKE_DIR}" PATH_VARS
    CMAKE_INSTALL_PREFIX CMAKE_INSTALL_LIBDIR CMAKE_INSTALL_INCLUDEDIR)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/ptex-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/ptex-config-version.cmake"
    DESTINATION "${CMAKE_DIR}" COMPONENT "devel")

install(EXPORT Ptex NAMESPACE Ptex::
    FILE "ptex-exports.cmake" DESTINATION "${CMAKE_DIR}" COMPONENT "devel")

if (IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    set(CMAKE_INSTALL_PKGCONFIG_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
else()
    set(CMAKE_INSTALL_PKGCONFIG_LIBDIR "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()
if (IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
    set(CMAKE_INSTALL_PKGCONFIG_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
else()
    set(CMAKE_INSTALL_PKGCONFIG_INCLUDEDIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif()

string(TOLOWER ${PROJECT_NAME} PROJECT_NAME_LOWERCASE)

if (PTEX_BUILD_SHARED_LIBS)
    get_target_property(PTEX_OUTPUT_NAME Ptex_dynamic OUTPUT_NAME)
else()
    get_target_property(PTEX_OUTPUT_NAME Ptex_static OUTPUT_NAME)
endif()
configure_file("${PROJECT_NAME_LOWERCASE}.pc.in" "${PROJECT_NAME_LOWERCASE}.pc" @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME_LOWERCASE}.pc"
    COMPONENT devel DESTINATION share/pkgconfig)
