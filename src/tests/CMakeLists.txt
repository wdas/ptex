add_definitions(-DPTEX_STATIC)

add_executable(wtest wtest.cpp)
target_link_libraries(wtest Ptex_dynamic)

add_executable(rtest rtest.cpp)
target_link_libraries(rtest Ptex_dynamic)

add_executable(ftest ftest.cpp)
target_link_libraries(ftest Ptex_dynamic)

add_executable(halftest halftest.cpp)
target_link_libraries(halftest Ptex_dynamic)

# create a function to add tests that compare output
# file results
function(add_compare_test test_name)
  add_test(NAME ${test_name}
    COMMAND ${CMAKE_COMMAND}
    -DOUT=${CMAKE_CURRENT_BINARY_DIR}/${test_name}.out
    -DDATA=${CMAKE_CURRENT_SOURCE_DIR}/${test_name}ok.dat
    -DCMD=$<TARGET_FILE:${test_name}>
    -P ${CMAKE_CURRENT_SOURCE_DIR}/compare_test.cmake)
endfunction(add_compare_test)

# add all the tests
add_test(NAME wtest COMMAND $<TARGET_FILE:wtest>)
add_compare_test(rtest)
add_compare_test(ftest)
add_test(NAME halftest COMMAND $<TARGET_FILE:halftest>)
