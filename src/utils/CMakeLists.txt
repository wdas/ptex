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
add_definitions(-DPTEX_VER="${PTEX_VER} \(${PTEX_SHA}\)")
target_link_libraries(ptxinfo Ptex_dynamic)

# installation
install ( TARGETS
  ptxinfo
  DESTINATION
  bin
  )
