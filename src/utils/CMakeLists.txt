# build version string from git query
execute_process(COMMAND git rev-parse HEAD
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                OUTPUT_VARIABLE PTEX_SHA
                OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND git describe ${PTEX_SHA}
                WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                OUTPUT_VARIABLE PTEX_VER
                OUTPUT_STRIP_TRAILING_WHITESPACE)

add_executable(ptxinfo ptxinfo.cpp)
add_definitions(-DPTEX_VER="${PTEX_VER} \(${PTEX_SHA}\)" -DPTEX_STATIC)

if(PTEX_BUILD_SHARED_LIBS)
  target_link_libraries(ptxinfo Ptex_dynamic ${ZLIB_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
elseif(PTEX_BUILD_STATIC_LIBS)
  target_link_libraries(ptxinfo Ptex_static ${ZLIB_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
endif()

install(TARGETS ptxinfo DESTINATION ${CMAKE_INSTALL_BINDIR})
