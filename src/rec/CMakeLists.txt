
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${LIBCOM_DIRS}
  ${CMAKE_SOURCE_DIR}/src/dbStatic
  ${CMAKE_SOURCE_DIR}/src/db
  ${CMAKE_BINARY_DIR}/src/db
  ${CMAKE_SOURCE_DIR}/src/ca
  ${CMAKE_SOURCE_DIR}/src/bpt
  ${CMAKE_BINARY_DIR}/src/bpt
  ${CMAKE_SOURCE_DIR}/src/registry
)

set(DBDFLAGS
-I ${CMAKE_CURRENT_SOURCE_DIR}
-I ${CMAKE_SOURCE_DIR}/src/db
)

set(REC_DBD_SRC
  aaiRecord
  aaoRecord
  aiRecord
  aoRecord
  aSubRecord
  biRecord
  boRecord
  calcRecord
  calcoutRecord
  compressRecord
  dfanoutRecord
  eventRecord
  fanoutRecord
  histogramRecord
  longinRecord
  longoutRecord
  mbbiRecord
  mbbiDirectRecord
  mbboRecord
  mbboDirectRecord
  permissiveRecord
  selRecord
  seqRecord
  stateRecord
  stringinRecord
  stringoutRecord
  subRecord
  subArrayRecord
  waveformRecord
)

set(LIB_HEADERS "" CACHE INTERNAL "xx" FORCE)
set(LIB_DBD "" CACHE INTERNAL "xx" FORCE)

foreach(REC ${REC_DBD_SRC})
  recordType(${REC}.dbd ${REC}.h)
  list(APPEND LIB_HEADERS ${CMAKE_CURRENT_BINARY_DIR}/${REC}.h)
  list(APPEND LIB_DBD ${REC}.dbd)
  set_source_files_properties(${REC}.c
    PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${REC}.h
  )
endforeach(REC)

add_custom_target(recdbd
  DEPENDS ${LIB_HEADERS} 
)

set(LIB_SRC
  aaiRecord.c
  aaoRecord.c
  aiRecord.c
  aoRecord.c
  aSubRecord.c
  biRecord.c
  boRecord.c
  calcRecord.c
  calcoutRecord.c
  compressRecord.c
  dfanoutRecord.c
  eventRecord.c
  fanoutRecord.c
  histogramRecord.c
  longinRecord.c
  longoutRecord.c
  mbbiRecord.c
  mbbiDirectRecord.c
  mbboRecord.c
  mbboDirectRecord.c
  permissiveRecord.c
  selRecord.c
  seqRecord.c
  stateRecord.c
  stringinRecord.c
  stringoutRecord.c
  subRecord.c
  subArrayRecord.c
  waveformRecord.c
)

set(LIB_DEP asIoc dbIoc registryIoc dbStaticIoc ca Com)

add_both_library(recIoc
  SONUM ${BASE_SOVERSION}
  SOURCES ${LIB_SRC}
  LINK_LIBRARIES ${LIB_DEP}
  EXTRA_DEP dbdbd
)

install(FILES ${LIB_HEADERS}
  DESTINATION ${EPICS_INSTALL_INCLUDE}
)
install(FILES ${LIB_DBD}
  DESTINATION ${EPICS_INSTALL_DBD}
)
