
include_directories(
  ${LIBCOM_DIRS}
  ${CMAKE_SOURCE_DIR}/src/dbStatic
)

set(LIB_HEADERS
  registryRecordType.h
  registryDeviceSupport.h
  registryDriverSupport.h
  registryFunction.h
  registryCommon.h
  registryIocRegister.h
)

set(LIB_SRCS
  registryRecordType.c
  registryDeviceSupport.c
  registryDriverSupport.c
  registryFunction.c
  registryCommon.c
  registryIocRegister.c
)

set(LIB_DEPS Com dbStaticIoc)

add_both_library(registryIoc
  SONUM ${BASE_SOVERSION}
  SOURCES ${LIB_SRCS}
  LINK_LIBRARIES ${LIB_DEPS}
)

if(HOST)
  install(FILES registerRecordDeviceDriver.pl
    DESTINATION ${EPICS_BIN_DIR}
  )
endif(HOST)

install(FILES ${LIB_HEADERS}
  DESTINATION ${EPICS_INCLUDE_DIR}
)
