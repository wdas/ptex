find_package(Doxygen)
if (DOXYGEN_FOUND)

    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/doxygen_log.txt
        COMMAND ${DOXYGEN_EXECUTABLE} Doxyfile >doxygen_log.txt
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS Doxyfile)
    add_custom_target(doc ALL DEPENDS doxygen_log.txt)

    # "make doxygen" forces doxygen to run
    add_custom_target(
        doxygen ${DOXYGEN_EXECUTABLE} Doxyfile >doxygen_log.txt
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS Doxyfile)

    install(DIRECTORY ptex/ DESTINATION ${CMAKE_INSTALL_DOCDIR})

endif (DOXYGEN_FOUND)
