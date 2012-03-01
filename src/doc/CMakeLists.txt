FIND_PACKAGE ( Doxygen )

FILE ( MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/../install/doc)

MESSAGE ( "${CMAKE_SOURCE_DIR} = " ${CMAKE_SOURCE_DIR} )

IF(DOXYGEN_FOUND)
  
  #  ADD_CUSTOM_TARGET(doc
  #	${DOXYGEN_EXECUTABLE} ${CMAKE_SOURCE_DIR}/doc/Doxyfile
  #	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/doc
  #	COMMENT "Generating API documentation with Doxygen" VERBATIM
  #	)
  
  SET (generated_doc_location ${CMAKE_SOURCE_DIR}/../install/doc/ptex/)
  SET (doxyfile_location ${CMAKE_SOURCE_DIR}/doc/)
  SET (perm  OWNER_READ GROUP_READ WORLD_READ)
  
  INSTALL(  
    CODE  
    "EXECUTE_PROCESS(  
      COMMAND \"${DOXYGEN_EXECUTABLE}\" ${doxyfile_location}/Doxyfile
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/doc
      )"
    )
  INSTALL(  
	DIRECTORY
	${generated_doc_location}  
    DESTINATION docs  
    FILE_PERMISSIONS ${perm}  
    ) 
  
ENDIF(DOXYGEN_FOUND)
