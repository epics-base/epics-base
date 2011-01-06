
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${LIBCOM_DIRS}
  ${CMAKE_SOURCE_DIR}/src/dbStatic
  ${CMAKE_SOURCE_DIR}/src/db
  ${CMAKE_SOURCE_DIR}/src/dbtools
  ${CMAKE_BINARY_DIR}/src/db
  ${CMAKE_SOURCE_DIR}/src/ca
  ${CMAKE_SOURCE_DIR}/src/as
  ${CMAKE_BINARY_DIR}/src/rec
  ${CMAKE_SOURCE_DIR}/src/rsrv
  ${CMAKE_SOURCE_DIR}/src/registry
)

set(LIB_DBD
  base.dbd
  system.dbd
  dlload.dbd
)

set(LIB_HEADERS
  epicsRelease.h
  iocInit.h
  miscIocRegister.h
  iocshRegisterCommon.h
)

set(LIB_SRC
  epicsRelease.c
  iocInit.c
  asSubRecordFunctions.c
  miscIocRegister.c
  dlload.c
  iocshRegisterCommon.c
)

set(LIB_DEP rsrvIoc asIoc dbtoolsIoc dbIoc registryIoc dbStaticIoc ca Com)

add_both_library(miscIoc
  SONUM ${BASE_SOVERSION}
  SOURCES ${LIB_SRC}
  LINK_LIBRARIES ${LIB_DEP}
)

add_dependencies(miscIocStatic recdbd dbdbd bptdbd)

install(FILES ${LIB_HEADERS}
  DESTINATION ${EPICS_INCLUDE_DIR}
)
install(FILES ${LIB_DBD}
  DESTINATION ${EPICS_DBD_DIR}
)
