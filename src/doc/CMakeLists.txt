find_package ( Doxygen )

file ( MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/../install/doc)

if (DOXYGEN_FOUND)
  set (generated_doc_location ${CMAKE_SOURCE_DIR}/../install/doc/ptex/)
  set (doxyfile_location ${CMAKE_SOURCE_DIR}/doc/)
  set (perm OWNER_READ GROUP_READ WORLD_READ)
  add_custom_target (doc
      ${DOXYGEN_EXECUTABLE} ${doxyfile_location}/Doxyfile
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/doc
    )
endif (DOXYGEN_FOUND)
