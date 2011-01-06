
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${LIBCOM_DIRS}
)

# antelope

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/y.tab.c
  COMMAND ${ANTELOPE} ${CMAKE_CURRENT_SOURCE_DIR}/dbYacc.y
  DEPENDS dbYacc.y
)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dbYacc.c
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/y.tab.c ${CMAKE_CURRENT_BINARY_DIR}/dbYacc.c
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/y.tab.c
)

# lex

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c
  COMMAND ${FLEX} -8 -I -S${FLEXSKEL} ${CMAKE_CURRENT_SOURCE_DIR}/dbLex.l
  DEPENDS dbLex.l
)
add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dbLex.c
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c ${CMAKE_CURRENT_BINARY_DIR}/dbLex.c
  DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.c
)

set_source_files_properties(dbYacc.c
  PROPERTIES OBJECT_DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/dbLex.c
)

set(LIB_HEADERS
  alarm.h
  alarmString.h
  dbBase.h
  dbFldTypes.h
  dbStaticLib.h
  dbStaticPvt.h
  link.h
  special.h
  guigroup.h
  devSup.h
  drvSup.h
  recSup.h
  dbStaticIocRegister.h
)

set(LIB_SRC
  dbStaticLib.c
  dbYacc.c
  dbPvdLib.c
)

set(HOST_SRCS
  ${LIB_SRC}
  dbStaticNoRun.c
)
set(HOST_DEPS Com)

set(IOC_SRCS
  ${LIB_SRC}
  dbStaticRun.c
  dbStaticIocRegister.c
)
set(IOC_DEPS Com)

add_both_library(dbStaticHost
  SONUM ${BASE_SOVERSION}
  SOURCES ${HOST_SRCS}
  LINK_LIBRARIES ${HOST_DEPS}
)
add_both_library(dbStaticIoc
  SONUM ${BASE_SOVERSION}
  SOURCES ${IOC_SRCS}
  LINK_LIBRARIES ${IOC_DEPS}
)

if(HOST)

  add_executable(dbReadTest
    dbReadTest.c
  )
  target_link_libraries(dbReadTest
    dbStaticHost${UTIL}
  )
  
  add_executable(dbExpand
    dbExpand.c
  )
  target_link_libraries(dbExpand
    dbStaticHost${UTIL}
  )
  
  add_executable(dbToMenuH
    dbToMenuH.c
  )
  target_link_libraries(dbToMenuH
    dbStaticHost${UTIL}
  )
  
  add_executable(dbToRecordtypeH
    dbToRecordtypeH.c
  )
  target_link_libraries(dbToRecordtypeH
    dbStaticHost${UTIL}
  )
  
  set_target_properties(
      dbReadTest dbExpand dbToMenuH dbToRecordtypeH
    PROPERTIES HAS_CXX Yes
  )

  install(TARGETS 
      dbReadTest dbExpand dbToMenuH dbToRecordtypeH
    RUNTIME DESTINATION ${EPICS_INSTALL_BIN}
    LIBRARY DESTINATION ${EPICS_INSTALL_LIB}
    ARCHIVE DESTINATION ${EPICS_INSTALL_LIB}
  )

endif(HOST)

install(FILES ${LIB_HEADERS}
  DESTINATION ${EPICS_INSTALL_INCLUDE}
)
