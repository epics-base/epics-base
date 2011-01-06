
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${LIBCOM_DIRS}
  ${CMAKE_SOURCE_DIR}/src/registry
  ${CMAKE_SOURCE_DIR}/src/dbStatic
  ${CMAKE_SOURCE_DIR}/src/db
  ${CMAKE_BINARY_DIR}/src/db
  ${CMAKE_SOURCE_DIR}/src/ca
  ${CMAKE_SOURCE_DIR}/src/as
  ${CMAKE_BINARY_DIR}/src/rec
  ${CMAKE_SOURCE_DIR}/src/misc
)

set(DBDFLAGS
  -I ${CMAKE_CURRENT_SOURCE_DIR}
  -I ${CMAKE_SOURCE_DIR}/src/misc
  -I ${CMAKE_SOURCE_DIR}/src/rec
  -I ${CMAKE_SOURCE_DIR}/src/db
  -I ${CMAKE_SOURCE_DIR}/src/bpt
  -I ${CMAKE_SOURCE_DIR}/src/dev/softDev
)

configure_file(epicsInstallDir.h.in ${CMAKE_CURRENT_BINARY_DIR}/epicsInstallDir.h)

set(LIB_HEADERS
  ${CMAKE_CURRENT_BINARY_DIR}/epicsInstallDir.h
)

set(LIB_DB
  softIocExit.db
)

expandDbd(softIoc.dbd
  ${CMAKE_SOURCE_DIR}/src/misc/base.dbd
  ${CMAKE_SOURCE_DIR}/src/misc/dlload.dbd
  ${CMAKE_SOURCE_DIR}/src/misc/system.dbd
)

registerRDD(${CMAKE_CURRENT_BINARY_DIR}/softIoc.dbd
  softIoc_registerRecordDeviceDriver
)

add_ioc_executable(softIoc
  SOURCES
    softMain.cpp
    softIoc_registerRecordDeviceDriver.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/epicsInstallDir.h
    ${CMAKE_CURRENT_BINARY_DIR}/softIoc.dbd
  LINK_LIBRARIES
    ${IOC_LIBRARIES}
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/epicsInstallDir.h
  DESTINATION ${EPICS_INSTALL_INCLUDE}
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/softIoc.dbd
  DESTINATION ${EPICS_INSTALL_DBD}
)
install(FILES ${LIB_DB}
  DESTINATION ${EPICS_INSTALL_DB}
)
