find_package ( Doxygen )

if (DOXYGEN_FOUND)
  set (generated_doc_location ${CMAKE_CURRENT_BINARY_DIR}/doc)
  set (doxyfile_location ${CMAKE_CURRENT_SOURCE_DIR} )
  set (perm OWNER_READ GROUP_READ WORLD_READ)
  file ( READ ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile doxygen_configuration_file )
  file ( WRITE ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile ${doxygen_configuration_file} )
  file ( APPEND ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile "OUTPUT_DIRECTORY = ${generated_doc_location}\n" )
  file ( APPEND ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile "INPUT = ${CMAKE_SOURCE_DIR}/ptex ${CMAKE_CURRENT_SOURCE_DIR}\n" )
  add_custom_target (doc ALL
      ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    )
  install ( DIRECTORY
    ${generated_doc_location}
    DESTINATION
    share
    )
endif (DOXYGEN_FOUND)
