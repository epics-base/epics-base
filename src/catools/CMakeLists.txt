
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${LIBCOM_DIRS}
  ${CMAKE_SOURCE_DIR}/src/dbStatic
  ${CMAKE_SOURCE_DIR}/src/db
  ${CMAKE_BINARY_DIR}/src/db
  ${CMAKE_SOURCE_DIR}/src/ca
)

add_library(catools STATIC
  tool_lib.c
)
target_link_libraries(catools
  ca${UTIL} Com${UTIL}
)

add_dependencies(catools
  dbIoc${UTIL}
)

add_executable(caget
  caget.c
)
target_link_libraries(caget
  catools
)

add_executable(caput
  caput.c
)
target_link_libraries(caput
  catools
)

add_executable(camonitor
  camonitor.c
)
target_link_libraries(camonitor
  catools
)

add_executable(cainfo
  cainfo.c
)
target_link_libraries(cainfo
  catools
)

set_target_properties(
    caget caput camonitor cainfo
  PROPERTIES HAS_CXX Yes
)

install(TARGETS caget caput camonitor cainfo
  RUNTIME DESTINATION ${EPICS_BIN_DIR}
)
