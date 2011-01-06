
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${LIBCOM_DIRS}
)

set(CA_HEADERS
  cadef.h
  caerr.h
  caeventmask.h
  caProto.h
  db_access.h
  addrList.h
  cacIO.h
  caDiagnostics.h
)

set(CA_SRC
  cac.cpp
  cacChannel.cpp
  cacChannelNotify.cpp
  cacContextNotify.cpp
  cacReadNotify.cpp
  cacWriteNotify.cpp
  cacStateNotify.cpp
  access.cpp
  iocinf.cpp
  convert.cpp
  test_event.cpp
  repeater.cpp
  searchTimer.cpp
  disconnectGovernorTimer.cpp
  repeaterSubscribeTimer.cpp
  baseNMIU.cpp
  nciu.cpp
  netiiu.cpp
  udpiiu.cpp
  tcpiiu.cpp
  noopiiu.cpp
  netReadNotifyIO.cpp
  netWriteNotifyIO.cpp
  netSubscription.cpp
  tcpSendWatchdog.cpp
  tcpRecvWatchdog.cpp
  bhe.cpp
  ca_client_context.cpp
  oldChannelNotify.cpp
  oldSubscription.cpp
  getCallback.cpp
  getCopy.cpp
  putCallback.cpp
  syncgrp.cpp
  CASG.cpp
  syncGroupNotify.cpp
  syncGroupReadNotify.cpp
  syncGroupWriteNotify.cpp
  localHostName.cpp
  comQueRecv.cpp
  comQueSend.cpp
  comBuf.cpp
  hostNameCache.cpp
  msgForMultiplyDefinedPV.cpp
)

if(OS_CLASS STREQUAL vxWorks)
  set(CA_SRC
    ${CA_SRC}
    templateInstances.cpp
  )
endif(OS_CLASS STREQUAL vxWorks)

set(LIB_DEPS Com)

add_both_library(ca
  SONUM ${BASE_SOVERSION}
  SOURCES ${CA_SRC}
  LINK_LIBRARIES ${LIB_DEPS}
)

install(FILES ${CA_HEADERS}
  DESTINATION ${EPICS_INSTALL_INCLUDE}
)
