project('epics-com', ['c', 'cpp'], version: '3.21.1', license: 'EPICS')

cc = meson.get_compiler('c')
compiler_id = cc.get_id()
system = host_machine.system()

dl_dep = cc.find_library('dl', required: false)
math_dep = cc.find_library('m', required: false)
thread_dep = dependency('threads')
valgrind_dep = dependency('valgrind')

perl_prog = find_program('perl', required: true)

api_header = custom_target(
 'libComAPI',
 input: ['../../src/tools/makeAPIheader.pl'],
 output: 'libComAPI.h',
 command: [perl_prog, '@INPUT0@', '-o', '@OUTPUT@', 'libCom'],
 install: true,
 install_dir: get_option('includedir'),
)

error_symbol_table = custom_target(
  'errSymTbl.c',
  input: ['src/error/makeStatTbl.pl', 'src/error/errSymTbl.h'],
  output: 'errSymTbl.c',
  command: [
    perl_prog,
    '@INPUT0@',
    '@CURRENT_SOURCE_DIR@/src/error/errMdef.h',
  ],
)

env_data = custom_target(
  'envData.c',
  input: ['src/env/bldEnvData.pl', files('src/env/envDefs.h')],
  output: 'envData.c',
  command: [
    perl_prog,
    '-I', join_paths(meson.current_source_dir(), '../../src/tools'),
    '@INPUT0@',
    '-o', '@OUTPUT@',
    '-e', '@INPUT1@',
    join_paths(meson.current_source_dir(), '../../configure'),
  ],
)

epics_version = custom_target(
 'epicsVersion.h',
 input: ['src/misc/makeEpicsVersion.pl', '../../configure/CONFIG_BASE_VERSION'],
 output: 'epicsVersion.h',
 command: [
   perl_prog,
   '@INPUT0@',
   '-o', '@OUTPUT@',
   '@INPUT1@',
 ],
 install: true,
 install_dir: get_option('includedir'),
)

epics_install_dir = custom_target(
 'epicsInstallDir.h',
 input: ['../database/src/std/softIoc/makeInstallDir.pl'],
 output: 'epicsInstallDir.h',
 command: [
   perl_prog,
   '@INPUT0@',
   get_option('prefix'),
 ],
 capture: true,
 install: true,
 install_dir: get_option('includedir'),
)

subdir('src/yacc')
subdir('src/flex')

headers = [
  'src/as/asLib.h',
  'src/as/asTrapWrite.h',

  'src/bucketLib/bucketLib.h',

  'src/calc/postfix.h',
  'src/calc/postfixPvt.h',

  'src/cvtFast/cvtFast.h',

  'src/cppStd/epicsAlgorithm.h',

  'src/cxxTemplates/epicsGuard.h',
  'src/cxxTemplates/epicsSingleton.h',
  'src/cxxTemplates/resourceLib.h',
  'src/cxxTemplates/tsDLList.h',
  'src/cxxTemplates/tsFreeList.h',
  'src/cxxTemplates/tsMinMax.h',
  'src/cxxTemplates/tsSLList.h',

  'src/dbmf/dbmf.h',

  'src/ellLib/ellLib.h',

  'src/env/envDefs.h',

  'src/error/epicsPrint.h',
  'src/error/errMdef.h',
  'src/error/errSymTbl.h',
  'src/error/errlog.h',

  'src/fdmgr/fdManager.h',
  'src/fdmgr/fdmgr.h',

  'src/freeList/freeList.h',

  'src/gpHash/gpHash.h',

  'src/iocsh/initHooks.h',
  'src/iocsh/iocsh.h',
  'src/iocsh/libComRegister.h',
  'src/iocsh/registry.h',

  'src/log/iocLog.h',
  'src/log/logClient.h',

  'src/macLib/macLib.h',

  'src/misc/adjustment.h',
  'src/misc/alarm.h',
  'src/misc/alarmString.h',
  'src/misc/cantProceed.h',
  'src/misc/dbDefs.h',
  'src/misc/epicsConvert.h',
  'src/misc/epicsExit.h',
  'src/misc/epicsExport.h',
  'src/misc/epicsStdlib.h',
  'src/misc/epicsString.h',
  'src/misc/epicsTypes.h',
  'src/misc/epicsUnitTest.h',

  'src/misc/ipAddrToAsciiAsynchronous.h',
  'src/misc/locationException.h',
  'src/misc/shareLib.h',
  'src/misc/testMain.h',

  'src/osi/compilerDependencies.h',
  'src/osi/devLib.h',
  'src/osi/devLibVME.h',
  'src/osi/devLibVMEImpl.h',
  'src/osi/epicsAssert.h',
  'src/osi/epicsAtomic.h',
  'src/osi/epicsAtomicDefault.h',
  'src/osi/epicsAtomicGCC.h',
  'src/osi/epicsEndian.h',
  'src/osi/epicsEvent.h',
  'src/osi/epicsFindSymbol.h',
  'src/osi/epicsGeneralTime.h',
  'src/osi/epicsInterrupt.h',
  'src/osi/epicsMessageQueue.h',
  'src/osi/epicsMutex.h',
  'src/osi/epicsReadline.h',
  'src/osi/epicsSignal.h',
  'src/osi/epicsSpin.h',
  'src/osi/epicsStackTrace.h',
  'src/osi/epicsStackTracePvt.h',
  'src/osi/epicsStdio.h',
  'src/osi/epicsStdioRedirect.h',
  'src/osi/epicsTempFile.h',
  'src/osi/epicsThread.h',
  'src/osi/epicsTime.h',
  'src/osi/generalTimeSup.h',
  'src/osi/osiClockTime.h',
  'src/osi/osiFileName.h',
  'src/osi/osiPoolStatus.h',
  'src/osi/osiProcess.h',
  'src/osi/osiSock.h',
  'src/osi/osiWireFormat.h',

  'src/pool/epicsThreadPool.h',
  'src/pool/poolPriv.h',

  'src/ring/epicsRingBytes.h',
  'src/ring/epicsRingPointer.h',

  'src/taskwd/taskwd.h',

  'src/timer/epicsTimer.h',
  'src/timer/timerPrivate.h',

  'src/yajl/yajl_alloc.h',
  'src/yajl/yajl_buf.h',
  'src/yajl/yajl_bytestack.h',
  'src/yajl/yajl_common.h',
  'src/yajl/yajl_encode.h',
  'src/yajl/yajl_gen.h',
  'src/yajl/yajl_lex.h',
  'src/yajl/yajl_parse.h',
  'src/yajl/yajl_parser.h',
]

# HACK: I would have liked to use meson's generators, but you cannot specify
# additional dependencies, and asLib.y needs asLib_lex.c to exist before being
# compiled.
# Another constraint is that the `depends:` key of `custom_target` cannot
# contain products from a meson generator, hence the use of `custom_target` for
# both.

aslib_lex = custom_target(
  'aslib_lex',
  input: ['src/as/asLib_lex.l'],
  output: ['asLib_lex.c'],
  command: [
    flex,
    '-o@OUTPUT@',
    '-S@CURRENT_SOURCE_DIR@/src/flex/flex.skel.static',
    '-8', '-I',
    '@INPUT@',
  ],
)

aslib_y = custom_target(
  'aslib_y',
  input: ['src/as/asLib.y'],
  output: ['asLib.tab.c', 'asLib.tab.h'],
  command: [
    yacc,
    '-b', join_paths(meson.current_build_dir(), '@BASENAME@'),
    '@INPUT@',
  ],
  depends: aslib_lex,
)

sources = [
  aslib_y,

  env_data,

  'src/as/asTrapWrite.c',

  'src/bucketLib/bucketLib.c',

  'src/calc/calcPerform.c',
  'src/calc/postfix.c',

  'src/cxxTemplates/epicsSingletonMutex.cpp',
  'src/cxxTemplates/resourceLib.cpp',

  'src/cvtFast/cvtFast.c',

  'src/dbmf/dbmf.c',

  'src/ellLib/ellLib.c',
  'src/ellLib/ellSort.c',

  'src/env/envSubr.c',

  'src/ring/epicsRingBytes.c',
  'src/ring/epicsRingPointer.cpp',

  'src/error/errSymLib.c',
  'src/error/errlog.c',
  error_symbol_table,

  'src/fdmgr/fdManager.cpp',
  'src/fdmgr/fdmgr.cpp',

  'src/freeList/freeListLib.c',

  'src/gpHash/gpHashLib.c',

  'src/iocsh/initHooks.c',
  'src/iocsh/iocsh.cpp',
  'src/iocsh/libComRegister.c',
  'src/iocsh/registry.c',

  'src/log/iocLog.c',
  'src/log/iocLogServer.c',
  'src/log/logClient.c',

  'src/macLib/macCore.c',
  'src/macLib/macEnv.c',
  'src/macLib/macUtil.c',

  'src/misc/aToIPAddr.c',
  'src/misc/adjustment.c',
  'src/misc/alarmString.c',
  'src/misc/cantProceed.c',
  'src/misc/epicsConvert.c',
  'src/misc/epicsExit.c',
  'src/misc/epicsStdlib.c',
  'src/misc/epicsString.c',
  'src/misc/epicsUnitTest.c',
  'src/misc/ipAddrToAsciiAsynchronous.cpp',
  'src/misc/truncateFile.c',

  'src/osi/devLibVME.c',
  'src/osi/epicsEvent.cpp',
  'src/osi/epicsGeneralTime.c',
  'src/osi/epicsMath.cpp',
  'src/osi/epicsMessageQueue.cpp',
  'src/osi/epicsMutex.cpp',
  'src/osi/epicsReadline.c',
  'src/osi/epicsStackTrace.c',
  'src/osi/epicsStdio.c',
  'src/osi/epicsThread.cpp',
  'src/osi/epicsTime.cpp',
  'src/osi/osiClockTime.c',
  'src/osi/osiSock.c',

  'src/pool/poolJob.c',
  'src/pool/threadPool.c',

  'src/taskwd/taskwd.c',

  'src/timer/epicsTimer.cpp',
  'src/timer/timer.cpp',
  'src/timer/timerQueue.cpp',
  'src/timer/timerQueueActive.cpp',
  'src/timer/timerQueueActiveMgr.cpp',
  'src/timer/timerQueuePassive.cpp',

  'src/yajl/yajl.c',
  'src/yajl/yajl_alloc.c',
  'src/yajl/yajl_buf.c',
  'src/yajl/yajl_encode.c',
  'src/yajl/yajl_gen.c',
  'src/yajl/yajl_lex.c',
  'src/yajl/yajl_parser.c',
]

include_directories = [
  'src/as',
  'src/calc',
  'src/bucketLib',
  'src/dbmf',
  'src/ellLib',
  'src/env',
  'src/fdmgr',
  'src/cvtFast',
  'src/cxxTemplates',
  'src/cppStd',
  'src/error',
  'src/freeList',
  'src/gpHash',
  'src/iocsh',
  'src/log',
  'src/macLib',
  'src/misc',
  'src/osi',
  'src/pool',
  'src/ring',
  'src/taskwd',
  'src/timer',
]

if compiler_id == 'gcc'
  include_directories += 'src/osi/compiler/gcc'

  headers += [
    'src/osi/compiler/gcc/compilerSpecific.h',
    'src/osi/compiler/gcc/epicsAtomicCD.h',
  ]
endif

if system == 'linux'
  include_directories += [
    'src/osi/os/Linux',
    'src/osi/os/posix',
    'src/osi/os/default',
  ]

  headers += [
    'src/osi/os/default/epicsGetopt.h',
    'src/osi/os/default/epicsMMIO.h',
    'src/osi/os/default/epicsMMIODef.h',
    'src/osi/os/default/osdInterrupt.h',
    'src/osi/os/default/osdMessageQueue.h',
    'src/osi/os/default/osdPoolStatus.h',
    'src/osi/os/default/osdVME.h',
    'src/osi/os/default/osdWireConfig.h',
    'src/osi/os/default/osdWireFormat.h',

    'src/osi/os/posix/epicsAtomicOSD.h',
    'src/osi/os/posix/epicsMath.h',
    'src/osi/os/posix/osdEvent.h',
    'src/osi/os/posix/osdMutex.h',
    'src/osi/os/posix/osdPosixMutexPriv.h',
    'src/osi/os/posix/osdStrtod.h',
    'src/osi/os/posix/osdThread.h',
    'src/osi/os/posix/osdTime.h',
    'src/osi/os/posix/osiUnistd.h',

    'src/osi/os/Linux/osdSock.h',
  ]

  sources += [
    'src/osi/os/default/devLibVMEOSD.c',
    'src/osi/os/default/epicsAtomicOSD.cpp',
    'src/osi/os/default/epicsSocketConvertErrnoToString.cpp',
    'src/osi/os/default/osdAssert.c',
    'src/osi/os/default/osdEnv.c',
    'src/osi/os/default/osdFindAddr.c',
    'src/osi/os/default/osdInterrupt.c',
    'src/osi/os/default/osdMessageQueue.cpp',
    'src/osi/os/default/osdPoolStatus.c',
    'src/osi/os/default/osdSockAddrReuse.cpp',
    'src/osi/os/default/osdThreadHooks.c',

    'src/osi/os/posix/epicsAtomicOSD.cpp',
    'src/osi/os/posix/epicsTempFile.c',
    'src/osi/os/posix/osdEvent.c',
    'src/osi/os/posix/osdFindSymbol.c',
    'src/osi/os/posix/osdMonotonic.c',
    'src/osi/os/posix/osdMutex.c',
    'src/osi/os/posix/osdProcess.c',
    'src/osi/os/posix/osdSignal.cpp',
    'src/osi/os/posix/osdSock.c',
    'src/osi/os/posix/osdSpin.c',
    'src/osi/os/posix/osdStdio.c',
    'src/osi/os/posix/osdThread.c',
    'src/osi/os/posix/osdTime.cpp',
    'src/osi/os/posix/systemCallIntMech.cpp',

    'src/osi/os/Linux/osdNetIntf.c',
    'src/osi/os/Linux/osdSockUnsentCount.c',
    'src/osi/os/Linux/osdgetexec.c',
    'src/osi/os/Linux/osdThreadExtra.c',
    'src/osi/os/Linux/osdBackTrace.cpp',
    # TODO: For some reason, it doesn't find the Dl_info symbol
    # 'src/osi/os/Linux/osdFindAddr.c',
  ]
endif

libcom = both_libraries(
  'epics-com',
  [sources, api_header, epics_version],
  include_directories: include_directories,
  dependencies: [dl_dep, math_dep, thread_dep, valgrind_dep],
  install: true,
)

epics_com_dep = declare_dependency(
  include_directories: include_directories,
  link_with: libcom,
)

tests = [
  ['aslibtest', 'test/aslibtest.c'],
  ['blocking_sock_test', 'test/blockingSockTest.cpp'],
  ['buck_test', 'test/buckTest.c'],
  # too long
  #['cvt_fast_perform', 'test/cvtFastPerform.cpp'],
  ['cvt_fast_test', 'test/cvtFastTest.c'],
  ['epics_algorithm_test', 'test/epicsAlgorithmTest.cpp'],
  ['epics_atomic_perform', 'test/epicsAtomicPerform.cpp'],
  # TODO: fails sometimes
  ['epics_atomic_test', 'test/epicsAtomicTest.cpp'],
  ['epics_calc_test', 'test/epicsCalcTest.cpp'],
  ['epics_ell_test', 'test/epicsEllTest.c'],
  ['epics_env_test', 'test/epicsEnvTest.c'],
  ['epics_env_unset_test', 'test/epicsEnvUnsetTest.c'],
  ['epics_errlog_test', 'test/epicsErrlogTest.c'],
  # too long
  #['epics_event_test', 'test/epicsEventTest.cpp'],
  ['epics_exit_test', 'test/epicsExitTest.c'],
  [
    'epics_inline_test',
    [
      'test/epicsInlineTest1.c',
      'test/epicsInlineTest2.c',
      'test/epicsInlineTest3.cpp',
      'test/epicsInlineTest4.cpp',
    ],
  ],
  ['epics_load_test', 'test/epicsLoadTest.cpp'],
  ['epics_mmio_test', 'test/epicsMMIOTest.c'],
  ['epics_math_test', 'test/epicsMathTest.c'],
  ['epics_max_threads', 'test/epicsMaxThreads.c'],
  # too long and failing?
  #['epics_message_queue_test', 'test/epicsMessageQueueTest.cpp'],
  ['epics_mutex_test', 'test/epicsMutexTest.cpp'],
  ['epics_sock_resolve_test', 'test/epicsSockResolveTest.c'],
  ['epics_spin_test', 'test/epicsSpinTest.c'],
  ['epics_stack_trace_test', 'test/epicsStackTraceTest.c'],
  ['epics_stdio_test', 'test/epicsStdioTest.c'],
  ['epics_stdlib_test', 'test/epicsStdlibTest.c'],
  ['epics_string_test', 'test/epicsStringTest.c'],
  ['epics_thread_class_test', 'test/epicsThreadClassTest.cpp'],
  ['epics_thread_hooks_test', 'test/epicsThreadHooksTest.c'],
  ['epics_thread_once_test', 'test/epicsThreadOnceTest.c'],
  ['epics_thread_perform', 'test/epicsThreadPerform.cpp'],
  ['epics_thread_pool_test', 'test/epicsThreadPoolTest.c'],
  ['epics_thread_priority_test', 'test/epicsThreadPriorityTest.cpp'],
  ['epics_thread_private_test', 'test/epicsThreadPrivateTest.cpp'],
  ['epics_thread_test', 'test/epicsThreadTest.cpp'],
  ['epics_time_test', 'test/epicsTimeTest.cpp'],
  ['epics_time_zone_test', 'test/epicsTimeZoneTest.c'],
  # too long
  #['epics_timer_test', 'test/epicsTimerTest.cpp'],
  ['epics_types_test', 'test/epicsTypesTest.c'],
  # TODO: test is expected to have some failures and some successes
  #['epics_unit_test_test', 'test/epicsUnitTestTest.c'],
  # depends on CA module (a reverse-dependency)
  #['fdmgr_test', 'test/fdmgrTest.c'],
  ['init_hook_test', 'test/initHookTest.c'],
  # TODO: Depends on hardcoded path to test data
  #['iocsh_test', 'test/iocshTest.cpp'],
  # TODO
  #['iocsh_test_bad_arg', 'test/iocshTestBadArg.cmd'],
  #['iocsh_test_bad_arg_indirect', 'test/iocshTestBadArgIndirect.cmd'],
  #['iocsh_test_success', 'test/iocshTestSuccess.cmd'],
  #['iocsh_test_success_indirect', 'test/iocshTestSuccessIndirect.cmd'],
  ['ip_addr_to_ascii_test', 'test/ipAddrToAsciiTest.cpp'],
  ['mac_def_expand_test', 'test/macDefExpandTest.c'],
  # TODO: has plt
  ['mac_lib_test', 'test/macLibTest.c'],
  ['non_epics_thread_priority_test', 'test/nonEpicsThreadPriorityTest.cpp'],
  # Depends on the network
  #['osi_sock_test', 'test/osiSockTest.c'],
  ['ring_bytes_test', 'test/ringBytesTest.c'],
  ['ring_pointer_test', 'test/ringPointerTest.c'],
  # too long
  #['taskwd_test', 'test/taskwdTest.c'],
  ['testexecname', 'test/testexecname.c'],
  # TODO: has plt and other things
  #['test/yajl_test.c'],
]

foreach t : tests
  exe = executable(
    [t[0], api_header, epics_install_dir],
    t[1],
    dependencies: [epics_com_dep, thread_dep],
    include_directories: '.',
  )
  test(t[0], exe, is_parallel: true)
endforeach

pkgconfig = import('pkgconfig')
pkgconfig.generate(libcom)

install_headers(headers)
