
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${LIBCOM_DIRS}
  ${CMAKE_SOURCE_DIR}/src/dbStatic
  ${CMAKE_SOURCE_DIR}/src/db
  ${CMAKE_SOURCE_DIR}/src/dbtools
  ${CMAKE_BINARY_DIR}/src/db
  ${CMAKE_SOURCE_DIR}/src/ca
  ${CMAKE_SOURCE_DIR}/src/bpt
  ${CMAKE_BINARY_DIR}/src/rec
)

set(LIB_DBD
  devSoft.dbd
)

set(LIB_SRC
  devAaiSoft.c
  devAaoSoft.c
  devAiSoft.c
  devAiSoftRaw.c
  devAoSoft.c
  devAoSoftRaw.c
  devBiSoft.c
  devBiSoftRaw.c
  devBoSoft.c
  devBoSoftRaw.c
  devCalcoutSoft.c
  devEventSoft.c
  devHistogramSoft.c
  devLiSoft.c
  devLoSoft.c
  devMbbiDirectSoft.c
  devMbbiDirectSoftRaw.c
  devMbbiSoft.c
  devMbbiSoftRaw.c
  devMbboDirectSoft.c
  devMbboDirectSoftRaw.c
  devMbboSoft.c
  devMbboSoftRaw.c
  devSASoft.c
  devSiSoft.c
  devSoSoft.c
  devWfSoft.c
  devGeneralTime.c

  devAoSoftCallback.c
  devBoSoftCallback.c
  devCalcoutSoftCallback.c
  devLoSoftCallback.c
  devMbboSoftCallback.c
  devMbboDirectSoftCallback.c
  devSoSoftCallback.c

  devTimestamp.c
  devSoStdio.c
)

set(LIB_DEP miscIoc recIoc asIoc dbIoc registryIoc dbStaticIoc ca Com)

add_both_library(softDevIoc
  SONUM ${BASE_SOVERSION}
  SOURCES ${LIB_SRC}
  LINK_LIBRARIES ${LIB_DEP}
)

install(FILES ${LIB_DBD}
  DESTINATION ${EPICS_DBD_DIR}
)
