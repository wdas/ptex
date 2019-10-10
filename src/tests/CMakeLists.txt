if(PTEX_BUILD_STATIC_LIBS)
    add_definitions(-DPTEX_STATIC)
endif()
add_executable(wtest wtest.cpp)
add_executable(rtest rtest.cpp)
add_executable(ftest ftest.cpp)
add_executable(halftest halftest.cpp)

target_link_libraries(wtest ${PTEX_LIBRARY})
target_link_libraries(rtest ${PTEX_LIBRARY})
target_link_libraries(ftest ${PTEX_LIBRARY})
target_link_libraries(halftest ${PTEX_LIBRARY})

# create a function to add tests that compare output
# file results
function(add_compare_test test_name)
  add_test(NAME ${test_name}
    COMMAND ${CMAKE_COMMAND}
    -DOUT=${CMAKE_CURRENT_BINARY_DIR}/${test_name}.out
    -DDATA=${CMAKE_CURRENT_SOURCE_DIR}/${test_name}ok.dat
    -DCMD=${CMAKE_CURRENT_BINARY_DIR}/${test_name}
    -P ${CMAKE_CURRENT_SOURCE_DIR}/compare_test.cmake)
endfunction(add_compare_test)

# add all the tests
add_test(NAME wtest COMMAND wtest)
add_compare_test(rtest)
add_compare_test(ftest)
add_test(NAME halftest COMMAND halftest)
