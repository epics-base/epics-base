
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${LIBCOM_DIRS}
  ${CMAKE_SOURCE_DIR}/src/dbStatic
  ${CMAKE_SOURCE_DIR}/src/db
  ${CMAKE_BINARY_DIR}/src/db
  ${CMAKE_SOURCE_DIR}/src/ca
  ${CMAKE_SOURCE_DIR}/src/as
)

set(LIB_HEADERS
  rsrv.h
  rsrvIocRegister.h
)

set(LIB_SRCS
  caserverio.c
  caservertask.c
  camsgtask.c
  camessage.c
  cast_server.c
  online_notify.c
  rsrvIocRegister.c
)

set(LIB_DEPS asIoc dbIoc ca Com)

add_both_library(rsrvIoc
  SONUM ${BASE_SOVERSION}
  SOURCES ${LIB_SRCS}
  LINK_LIBRARIES ${LIB_DEPS}
)

install(FILES ${LIB_HEADERS}
  DESTINATION ${EPICS_INSTALL_INCLUDE}
)
