project('epics-ca', ['c', 'cpp'], version: '4.14.2', license: 'EPICS')

epics_com_dep = dependency('epics-com')

ca_version = configure_file(
  input: './src/client/caVersion.h@',
  output: 'caVersion.h',
  configuration: configuration_data({
    'EPICS_CA_MAJOR_VERSION': '4',
    'EPICS_CA_MINOR_VERSION': '14',
    'EPICS_CA_MAINTENANCE_VERSION': '2',
    'EPICS_CA_DEVELOPMENT_FLAG': '1',
  }),
)

perl_prog = find_program('perl', required: true)
api_header = custom_target(
  'libCaAPI',
  input: ['../../src/tools/makeAPIheader.pl'],
  output: 'libCaAPI.h',
  command: [perl_prog, '@INPUT0@', '-o', '@OUTPUT@', 'libCa'],
  install: true,
  install_dir: get_option('includedir'),
)

headers = [
  'src/client/cadef.h',
  'src/client/caerr.h',
  'src/client/caeventmask.h',
  'src/client/caProto.h',
  'src/client/db_access.h',
  'src/client/addrList.h',
  'src/client/cacIO.h',
  'src/client/caDiagnostics.h',
  'src/client/net_convert.h',
  ca_version,
]

sources = [
  api_header,
  'src/client/cac.cpp',
  'src/client/cacChannel.cpp',
  'src/client/cacChannelNotify.cpp',
  'src/client/cacContextNotify.cpp',
  'src/client/cacReadNotify.cpp',
  'src/client/cacWriteNotify.cpp',
  'src/client/cacStateNotify.cpp',
  'src/client/access.cpp',
  'src/client/iocinf.cpp',
  'src/client/convert.cpp',
  'src/client/test_event.cpp',
  'src/client/repeater.cpp',
  'src/client/searchTimer.cpp',
  'src/client/disconnectGovernorTimer.cpp',
  'src/client/repeaterSubscribeTimer.cpp',
  'src/client/baseNMIU.cpp',
  'src/client/nciu.cpp',
  'src/client/netiiu.cpp',
  'src/client/udpiiu.cpp',
  'src/client/tcpiiu.cpp',
  'src/client/noopiiu.cpp',
  'src/client/netReadNotifyIO.cpp',
  'src/client/netWriteNotifyIO.cpp',
  'src/client/netSubscription.cpp',
  'src/client/tcpSendWatchdog.cpp',
  'src/client/tcpRecvWatchdog.cpp',
  'src/client/bhe.cpp',
  'src/client/ca_client_context.cpp',
  'src/client/oldChannelNotify.cpp',
  'src/client/oldSubscription.cpp',
  'src/client/getCallback.cpp',
  'src/client/getCopy.cpp',
  'src/client/putCallback.cpp',
  'src/client/syncgrp.cpp',
  'src/client/CASG.cpp',
  'src/client/syncGroupNotify.cpp',
  'src/client/syncGroupReadNotify.cpp',
  'src/client/syncGroupWriteNotify.cpp',
  'src/client/localHostName.cpp',
  'src/client/comQueRecv.cpp',
  'src/client/comQueSend.cpp',
  'src/client/comBuf.cpp',
  'src/client/hostNameCache.cpp',
  'src/client/msgForMultiplyDefinedPV.cpp',
]

include_directories = [
  'src/client'
]

libca = both_libraries(
  'epics-ca',
  sources,
  include_directories: include_directories,
  dependencies: [epics_com_dep],
  install: true,
)

epics_ca_dep = declare_dependency(
  include_directories: include_directories,
  link_with: libca,
)

# TODO: do we include the "test" tools in ./src/client?

foreach e : [
  ['caget', 'src/tools/caget.c'],
  ['cainfo', 'src/tools/cainfo.c'],
  ['camonitor', 'src/tools/camonitor.c'],
]
  executable(
    [e[0], api_header],
    [e[1], 'src/tools/tool_lib.c'],
    include_directories: 'src/tools',
    dependencies: [epics_ca_dep, epics_com_dep],
    install: true,
  )
endforeach

pkgconfig = import('pkgconfig')
pkgconfig.generate(libca)

install_headers(headers)
