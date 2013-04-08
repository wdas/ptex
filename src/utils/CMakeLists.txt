# build version string from git query
execute_process(COMMAND git rev-list --max-count=1 HEAD
                OUTPUT_VARIABLE PTEX_SHA
                OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND git describe ${PTEX_SHA}
                OUTPUT_VARIABLE PTEX_VER
                OUTPUT_STRIP_TRAILING_WHITESPACE)

add_executable(ptxinfo ptxinfo.cpp)
add_definitions(-DPTEX_VER="${PTEX_VER} \(${PTEX_SHA}\)")
target_link_libraries(ptxinfo Ptex_dynamic)
