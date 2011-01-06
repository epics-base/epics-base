
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/generic
  ${CMAKE_CURRENT_SOURCE_DIR}/generic/st
  ${CMAKE_CURRENT_SOURCE_DIR}/io/bsdSocket
  ${LIBCOM_DIRS}
  ${CMAKE_SOURCE_DIR}/src/dbStatic
  ${CMAKE_SOURCE_DIR}/src/db
  ${CMAKE_BINARY_DIR}/src/db
  ${CMAKE_SOURCE_DIR}/src/ca
  ${CMAKE_SOURCE_DIR}/src/gdd
  ${CMAKE_BINARY_DIR}/src/gdd
)

set(LIB_HEADERS
  generic/casdef.h
  generic/casEventMask.h
  generic/caNetAddr.h
)

set(LIB_SRCS
  generic/caServer.cc
  generic/caServerI.cc
  generic/casCoreClient.cc
  generic/casDGClient.cc
  generic/casStrmClient.cc
  generic/casPV.cc
  generic/casPVI.cc
  generic/casChannel.cc
  generic/casChannelI.cc
  generic/casAsyncIOI.cc
  generic/casAsyncReadIO.cc
  generic/casAsyncReadIOI.cc
  generic/casAsyncWriteIO.cc
  generic/casAsyncWriteIOI.cpp
  generic/casAsyncPVExistIO.cc
  generic/casAsyncPVExistIOI.cpp
  generic/casAsyncPVAttachIO.cc
  generic/casAsyncPVAttachIOI.cpp
  generic/casEventSys.cc
  generic/casMonitor.cc
  generic/casMonEvent.cc
  generic/inBuf.cc
  generic/outBuf.cc
  generic/casCtx.cc 
  generic/casEventMask.cc

  generic/casBufferFactory.cpp
  generic/pvExistReturn.cc
  generic/pvAttachReturn.cc
  generic/caNetAddr.cc
  generic/beaconTimer.cc
  generic/beaconAnomalyGovernor.cc
  generic/clientBufMemoryManager.cpp
  generic/chanIntfForPV.cc
  generic/channelDestroyEvent.cpp

  generic/st/ioBlocked.cc
  generic/st/casIntfOS.cc
  generic/st/casDGIntfOS.cc
  generic/st/casStreamOS.cc

  io/bsdSocket/caServerIO.cc
  io/bsdSocket/casIntfIO.cc
  io/bsdSocket/casDGIntfIO.cc
  io/bsdSocket/casStreamIO.cc
  io/bsdSocket/ipIgnoreEntry.cpp
)

set(LIB_DEPS ca gdd Com)

add_both_library(cas
  SONUM ${BASE_SOVERSION}
  SOURCES ${LIB_SRCS}
  LINK_LIBRARIES ${LIB_DEPS}
)

add_dependencies(cas
  dbIoc${UTIL}
)

install(FILES ${LIB_HEADERS}
  DESTINATION ${EPICS_INSTALL_INCLUDE}
)
