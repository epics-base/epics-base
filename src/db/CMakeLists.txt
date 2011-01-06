
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${LIBCOM_DIRS}
  ${CMAKE_SOURCE_DIR}/src/dbStatic
  ${CMAKE_BINARY_DIR}/src/dbStatic
  ${CMAKE_SOURCE_DIR}/src/ca
  ${CMAKE_SOURCE_DIR}/src/bpt
)

set(DBDFLAGS
  -I ${CMAKE_CURRENT_SOURCE_DIR}
)

set(LIB_HEADERS
  callback.h
  dbAccess.h
  dbAccessDefs.h
  dbCa.h
  dbAddr.h
  dbBkpt.h
  dbConvert.h
  dbConvertFast.h
  dbEvent.h
  dbLock.h
  dbNotify.h
  dbScan.h
  dbTest.h
  dbCaTest.h
  db_test.h
  db_field_log.h
  initHooks.h
  recGbl.h
  dbIocRegister.h
# The following go away what old database access goes away
  db_access_routines.h
  db_convert.h

  ${CMAKE_CURRENT_BINARY_DIR}/dbCommon.h
)

recordType(dbCommonRecord.dbd dbCommon.h)
set_source_files_properties(dbCommonRecord.dbd
  PROPERTIES OBJECT_DEPENDS dbCommon.dbd
)

set(LIB_DBDS
  dbCommon.dbd
  menuGlobal.dbd
)

set(MENUES
  menuAlarmSevr
  menuAlarmStat
  menuCompress
  menuFtype
  menuIvoa
  menuOmsl
  menuPini
  menuPriority
  menuScan
  menuYesNo
  menuSimm
)

foreach(menu ${MENUES})
  menuDbd(${menu}.dbd ${menu}.h)
  list(APPEND LIB_DBDS ${menu}.dbd)
  list(APPEND LIB_HEADERS ${CMAKE_CURRENT_BINARY_DIR}/${menu}.h)
endforeach(menu)

add_dependencies(dbdbd ${LIB_HEADERS})

set(LIB_SRCS
  dbLock.c
  dbAccess.c
  dbBkpt.c
  dbConvert.c
  dbFastLinkConv.c
  dbNotify.c
  dbScan.c
  dbEvent.c
  dbTest.c
  db_access.c
  db_test.c
  recGbl.c
  callback.c
  dbCa.c
  dbCaTest.c
  initHooks.c
  cvtBpt.c
  dbContext.cpp
  dbChannelIO.cpp
  dbSubscriptionIO.cpp
  dbPutNotifyBlocker.cpp
  dbContextReadNotifyCache.cpp
  templateInstances.cpp
  dbIocRegister.c
  ${LIB_HEADERS}
)

set(LIB_DEPS dbStaticIoc ca Com)

add_both_library(dbIoc
  SONUM ${BASE_SOVERSION}
  SOURCES ${LIB_SRCS}
  LINK_LIBRARIES ${LIB_DEPS}
)

install(FILES ${LIB_HEADERS}
  DESTINATION ${EPICS_INCLUDE_DIR}
)
install(FILES ${LIB_DBDS}
  DESTINATION ${EPICS_DBD_DIR}
)
