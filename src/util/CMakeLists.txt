
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${LIBCOM_DIRS}
  ${CMAKE_SOURCE_DIR}/src/ca
)

add_executable(ca_test
  ca_test_main.c
  ca_test.c
)
target_link_libraries(ca_test
  ca${UTIL}
)

add_executable(iocLogServer
  iocLogServer.c
)
target_link_libraries(iocLogServer
  Com${UTIL}
)

set_target_properties(ca_test iocLogServer
  PROPERTIES HAS_CXX YES
)

install(TARGETS ca_test iocLogServer
  RUNTIME DESTINATION ${EPICS_INSTALL_BIN}
)
