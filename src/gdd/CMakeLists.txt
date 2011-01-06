include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${LIBCOM_DIRS}
  ${CMAKE_SOURCE_DIR}/src/ca
)

set(LIB_HEADERS
  gdd.h
  gddI.h
  gddContainer.h
  gddContainerI.h
  gddArray.h
  gddArrayI.h
  gddScalar.h
  gddScalarI.h
  gddNewDel.h
  gddUtils.h
  gddUtilsI.h
  gddErrorCodes.h
  aitTypes.h
  aitConvert.h
  aitHelpers.h
  dbMapper.h
  gddAppTable.h
  gddAppFuncTable.h
  smartGDDPointer.h
  ${CMAKE_CURRENT_BINARY_DIR}/gddApps.h
  gddEnumStringTable.h
)

set(AITGENSRCS
  gdd.cc
  gddTest.cc
  gddAppTable.cc
  gddNewDel.cc
  gddAppDefs.cc
  aitTypes.c
  aitConvert.cc
  aitHelpers.cc
  gddArray.cc
  gddContainer.cc
  gddErrorCodes.cc
  gddUtils.cc
  gddEnumStringTable.cc
)

set(gdd_SRCS
  gdd.cc
  gddTest.cc
  gddAppTable.cc
  gddNewDel.cc
  gddAppDefs.cc
  aitTypes.c
  aitConvert.cc
  aitHelpers.cc
  dbMapper.cc
  gddArray.cc
  gddContainer.cc
  gddErrorCodes.cc
  gddUtils.cc
  gddEnumStringTable.cc
  ${CMAKE_CURRENT_BINARY_DIR}/gddApps.h
)

set(gdd_DEPS Com)

add_both_library(gdd
  SONUM ${BASE_SOVERSION}
  SOURCES ${gdd_SRCS}
  LINK_LIBRARIES ${gdd_DEPS}
)

if(HOST)
  add_executable(aitGen
    aitTypes.c
    aitGen.c
  )
  target_link_libraries(aitGen Com${UTIL})
  
  add_executable(genApps
    genApps.cc
    ${AITGENSRCS}
  )
  target_link_libraries(genApps Com${UTIL})

  install(TARGETS aitGen genApps
    RUNTIME DESTINATION ${EPICS_INSTALL_BIN}
  )

endif(HOST)

add_custom_command(OUTPUT gddApps.h
  COMMAND ${GENAPPS} gddApps.h
  DEPENDS ${GENAPPS} # OTHERS???
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_command(OUTPUT aitConvertGenerated.cc
  COMMAND ${AITGEN} aitConvertGenerated.cc
  DEPENDS ${AITGEN} # OTHERS???
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_source_files_properties(aitConvert.cc
  PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/aitConvertGenerated.cc
)

install(FILES ${LIB_HEADERS}
  DESTINATION ${EPICS_INSTALL_INCLUDE}
)
