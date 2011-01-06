
configure_file(epicsVersion.h.in ${CMAKE_CURRENT_BINARY_DIR}/epicsVersion.h)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/envData.c
  COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/env/bldEnvData.pl ${EPICS_CONFIG} ${CMAKE_CURRENT_SOURCE_DIR}/env/envDefs.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/env/envDefs.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set(ERR_S_SRC
  ${CMAKE_SOURCE_DIR}/src/libCom/osi/devLib.h
  ${CMAKE_SOURCE_DIR}/src/as/asLib.h
  ${CMAKE_SOURCE_DIR}/src/db/dbAccessDefs.h
  ${CMAKE_SOURCE_DIR}/src/dbStatic/devSup.h
  ${CMAKE_SOURCE_DIR}/src/dbStatic/drvSup.h
  ${CMAKE_SOURCE_DIR}/src/dbStatic/recSup.h
  ${CMAKE_SOURCE_DIR}/src/dbStatic/dbStaticLib.h
  ${CMAKE_CURRENT_SOURCE_DIR}/error/errMdef.h
  ${CMAKE_SOURCE_DIR}/src/cas/generic/casdef.h
  ${CMAKE_SOURCE_DIR}/src/gdd/gddAppFuncTable.h
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/errSymTbl.c
  COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/error/makeStatTbl.pl ${ERR_S_SRC}
  DEPENDS ${ERR_S_SRC}
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

include_directories(
  ${LIBCOM_DIRS}
)

# OSD

set(OSD_HEADER_NAMES
  camacLib.h
  epicsDynLink.h
  epicsGetopt.h
  epicsMath.h
  epicsReadline.h
  module_types.h
  osdEvent.h
  osdInterrupt.h
  osdMessageQueue.h
  osdMutex.h
  osdPoolStatus.h
  osdSock.h
  osdStrtod.h
  osdThread.h
  osdTime.h
  osdVME.h
  osdWireConfig.h
  osdWireFormat.h
  osiFileName.h
  osiSock.h
  osiUnistd.h
  task_params.h
  ucx.h
)

set(OSD_SRC_NAMES
  atReboot
  devLibVMEOSD
  epicsDynLink
  epicsGetopt
  epicsReadline
  epicsSocketConvertErrnoToString
  epicsTempFile
  forceBadAllocException
  logMsgToErrlog
  osdAssert
  osdEnv
  osdEvent
  osdFindSymbol
  osdInterrupt
  osdMessageQueue
  osdMutex
  osdNetIntf
  osdPoolStatus
  osdProcess
  osdSignal
  osdSock
  osdSockAddrReuse
  osdStdio
  osdThread
  osdTime
  setThreadName
  systemCallIntMech
  veclist
  vxComLibrary
)

#OSD is special in that several files with the same or
#similar names (file.c and file.cpp) will exist in the
#search path.
#Find the first one

foreach(OSDH ${OSD_HEADER_NAMES})
 foreach(OSDIR ${EPICS_OSI_DIRS})
    if(EXISTS ${OSDIR}/${OSDH})
      list(APPEND OSD_HEADERS ${OSDIR}/${OSDH})
      break()
    endif(EXISTS ${OSDIR}/${OSDH})
  endforeach(OSDIR)
endforeach(OSDH)
#message(STATUS "OSDH: ${OSD_HEADERS}")

foreach(OSDS ${OSD_SRC_NAMES})
  foreach(OSDIR ${EPICS_OSI_DIRS})
    if(EXISTS ${OSDIR}/${OSDS}.c)
      list(APPEND OSD_SRCS ${OSDIR}/${OSDS}.c)
      break()
    elseif(EXISTS ${OSDIR}/${OSDS}.cpp)
      list(APPEND OSD_SRCS ${OSDIR}/${OSDS}.cpp)
      break()
    endif(EXISTS ${OSDIR}/${OSDS}.c)
  endforeach(OSDIR)
endforeach(OSDS)
#message(STATUS "OSDS: ${OSD_SRCS}")

# OSI

set(OSI_HEADERS
  osi/osiSock.h
  osi/epicsInterrupt.h
  osi/epicsMutex.h
  osi/epicsEvent.h
  osi/epicsAssert.h
  osi/epicsFindSymbol.h
  osi/osiPoolStatus.h
  osi/epicsThread.h
  osi/epicsTime.h
  osi/epicsGeneralTime.h
  osi/generalTimeSup.h
  osi/osiClockTime.h
  osi/epicsSignal.h
  osi/osiProcess.h
  osi/osiWireFormat.h
  osi/epicsEndian.h
  osi/epicsMessageQueue.h
  osi/epicsStdio.h
  osi/epicsStdioRedirect.h
  osi/devLib.h
  osi/devLibVME.h
  osi/devLibVMEImpl.h
)

set(OSI_SRC

  osi/epicsThread.cpp
  osi/epicsMutex.cpp
  osi/epicsEvent.cpp
  osi/epicsTime.cpp
  osi/epicsMessageQueue.cpp
  osi/epicsMath.cpp

  osi/epicsGeneralTime.c
  osi/osiClockTime.c
  osi/osiSock.c

  osi/epicsStdio.c
  osi/devLibVME.c

  ${OSD_SRCS}
)

# Everything else

set(COM_HEADERS
  ${CMAKE_CURRENT_BINARY_DIR}/epicsVersion.h

  bucketLib/bucketLib.h

  ring/epicsRingPointer.h
  ring/epicsRingBytes.h

  calc/postfix.h

  cppStd/epicsAlgorithm.h
  cppStd/epicsExcept.h
  cppStd/epicsMemory.h

  cvtFast/cvtFast.h

  cxxTemplates/resourceLib.h
  cxxTemplates/tsDLList.h
  cxxTemplates/tsSLList.h
  cxxTemplates/tsMinMax.h
  cxxTemplates/tsBTree.h
  cxxTemplates/tsFreeList.h
  cxxTemplates/epicsSingleton.h
  cxxTemplates/epicsGuard.h
  cxxTemplates/epicsOnce.h

  dbmf/dbmf.h

  ellLib/ellLib.h

  env/envDefs.h

  error/epicsPrint.h
  error/errMdef.h
  error/errSymTbl.h
  error/errlog.h
  error/error.h

  fdmgr/fdManager.h
  fdmgr/fdmgr.h

  freeList/freeList.h

  gpHash/gpHash.h

  iocsh/iocsh.h
  iocsh/registry.h
  iocsh/libComRegister.h

  logClient/iocLog.h
  logClient/logClient.h

  macLib/macLib.h

  misc/adjustment.h
  misc/cantProceed.h
  misc/dbDefs.h
  misc/epicsConvert.h
  misc/epicsExit.h
  misc/epicsStdlib.h
  misc/epicsString.h
  misc/epicsTypes.h
  misc/shareLib.h
  misc/epicsExport.h
  misc/unixFileName.h
  misc/locationException.h
  misc/ipAddrToAsciiAsynchronous.h
  misc/compilerDependencies.h
  misc/epicsUnitTest.h
  misc/testMain.h

  taskwd/taskwd.h

  timer/epicsTimer.h

  tsDefs/tsDefs.h
)

set(COM_SRCS
  bucketLib/bucketLib.c

  ring/epicsRingPointer.cpp
  ring/epicsRingBytes.c

  calc/postfix.c
  calc/calcPerform.c

  cvtFast/cvtFast.c

  cxxTemplates/resourceLib.cpp
  cxxTemplates/epicsOnce.cpp
  cxxTemplates/epicsSingletonMutex.cpp

  dbmf/dbmf.c

  ellLib/ellLib.c

  env/envSubr.c
  ${CMAKE_CURRENT_BINARY_DIR}/envData.c

  error/errlog.c
  error/errSymLib.c
  ${CMAKE_CURRENT_BINARY_DIR}/errSymTbl.c

  fdmgr/fdmgr.cpp
  fdmgr/fdManager.cpp

  freeList/freeListLib.c
  #freeList/freeList/freeList.html

  gpHash/gpHashLib.c
  #gpHash/gpHash/gpHash.html

  iocsh/iocsh.cpp
  iocsh/registry.c
  iocsh/libComRegister.c

  logClient/iocLog.c
  logClient/logClient.c

  macLib/macCore.c
  macLib/macEnv.c
  macLib/macUtil.c

  misc/aToIPAddr.c
  misc/adjustment.c
  misc/cantProceed.c
  misc/epicsConvert.c
  misc/epicsExit.c
  misc/epicsStdlib.c
  misc/epicsString.c
  misc/truncateFile.c
  misc/ipAddrToAsciiAsynchronous.cpp
  misc/epicsUnitTest.c

  ${OSI_SRC}

  taskwd/taskwd.c

  timer/epicsTimer.cpp
  timer/timer.cpp
  timer/timerQueue.cpp
  timer/timerQueueActive.cpp
  timer/timerQueueActiveMgr.cpp
  timer/timerQueuePassive.cpp

#tsDefs contains R3.13 compatibility tsStamp code
  tsDefs/tsDefs.c
)

if(RTEMS)
  set(COM_SRCS
    ${COM_SRCS}
    osi/osiNTPTime.c
  )
elseif(OS_CLASS STREQUAL vxWorks)
  set(COM_SRC
    ${COM_SRC}
    osi/osiNTPTime.c
  )
endif(RTEMS)

if(LIBRT AND LIBDL)
  list(APPEND LDADD ${LIBRT} ${LIBDL})
endif(LIBRT AND LIBDL)

if(WIN32 AND NOT CYGWIN)
  list(APPEND LDADD ws2_32)
endif(WIN32 AND NOT CYGWIN)

add_both_library(Com
  SONUM ${BASE_SOVERSION}
  SOURCES ${COM_SRCS}
  LINK_LIBRARIES ${LIB_DEPS}
  EXTRA_LIB ${CMAKE_THREAD_LIBS_INIT}
            ${LDADD}
)

install(FILES ${COM_HEADERS} ${OSI_HEADERS}
  DESTINATION ${EPICS_INSTALL_INCLUDE}
)
install(FILES ${OSD_HEADERS}
  DESTINATION ${EPICS_INSTALL_INCLUDE_OS}
)
