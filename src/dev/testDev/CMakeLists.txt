
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${LIBCOM_DIRS}
  ${CMAKE_SOURCE_DIR}/src/dbStatic
  ${CMAKE_SOURCE_DIR}/src/db
  ${CMAKE_BINARY_DIR}/src/db
  ${CMAKE_SOURCE_DIR}/src/ca
  ${CMAKE_SOURCE_DIR}/src/bpt
  ${CMAKE_BINARY_DIR}/src/rec
)

set(LIB_DBD
  devTestAsyn.dbd
)

set(LIB_SRC
  devHistogramTestAsyn.c
  devTestAsyn.c
)

set(LIB_DEP miscIoc recIoc asIoc dbIoc registryIoc dbStaticIoc ca Com)

add_both_library(testDevIoc
  SONUM ${BASE_SOVERSION}
  SOURCES ${LIB_SRC}
  LINK_LIBRARIES ${LIB_DEP}
)

install(FILES ${LIB_DBD}
  DESTINATION ${EPICS_DBD_DIR}
)
