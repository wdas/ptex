message("Run ${CMD} to produce: ${OUT} and compare with: ${DATA}")
# run the test
execute_process(COMMAND "${CMD}" OUTPUT_FILE "${OUT}" RESULT_VARIABLE ret)
if(NOT ${ret} EQUAL 0)
  message(FATAL_ERROR "${CMD} returned a non-zero value:${ret}")
endif()
# use cmake executable to compare the output of the tests
execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${OUT}" "${DATA}" 
  RESULT_VARIABLE ret)
if(NOT ${ret} EQUAL 0)
  message(FATAL_ERROR
    "compare_files ${OUT} ${DATA} returned a non-zero value:${ret}")
endif()