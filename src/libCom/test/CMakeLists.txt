
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${LIBCOM_DIRS}
)

set(IOCTESTS "" CACHE INTERNAL "xx" FORCE)

macro(baseTest test)
  if(HOST)
    add_executable(${test}Test ${ARGN})
    target_link_libraries(${test}Test Com${UTIL})
    add_test(${test} ${test}Test)
    list(APPEND TESTTARGETS ${test}Test)
  else(HOST)
    list(APPEND IOCTESTS ${ARGN})
  endif(HOST)
endmacro(baseTest)

if(HOST)
  baseTest(epicsUnitTest epicsUnitTestTest.c)
endif(HOST)

baseTest(epicsCalc epicsCalcTest.cpp)
baseTest(cvtFastPerform cvtFastPerform.cpp)
baseTest(epicsAlgorithm epicsAlgorithmTest.cpp)
baseTest(epicsErrlog epicsErrlogTest.c)
baseTest(epicsMath epicsMathTest.c)
baseTest(epicsMessageQueueTest epicsMessageQueueTest.cpp)
baseTest(epicsEll epicsEllTest.c)
baseTest(epicsStdio epicsStdioTest.c)
baseTest(epicsStringTest epicsStringTest.c)
baseTest(epicsThreadPriority epicsThreadPriorityTest.cpp)
baseTest(epicsThreadPrivate epicsThreadPrivateTest.cpp)
baseTest(epicsExit epicsExitTest.c)
baseTest(epicsTime epicsTimeTest.cpp)
baseTest(epicsTimer epicsTimerTest.cpp)
baseTest(epicsThreadTest epicsThreadTest.cpp)
baseTest(epicsThreadOnceTest epicsThreadOnceTest.c)
baseTest(ringPointer ringPointerTest.c)
baseTest(ringBytes ringBytesTest.c)
baseTest(epicsEvent epicsEventTest.cpp)
baseTest(epicsMutex epicsMutexTest.cpp)
baseTest(epicsException epicsExceptionTest.cpp)
baseTest(macEnvExpand macEnvExpandTest.c)
baseTest(macLib macLibTest.c)
baseTest(taskwd taskwdTest.c)
baseTest(blockingSock blockingSockTest.cpp)

if(HOST)
  set_target_properties(
      ${TESTTARGETS}
    PROPERTIES HAS_CXX Yes
  )

  add_custom_target(tests-build
    DEPENDS ${TESTTARGETS}
  )
elseif(IOC)

  if(RTEMS)
    list(APPEND IOCTESTS rtemsTestHarness.c)
  endif(RTEMS)

  add_ioc_executable(iocTestHarness
    SOURCES epicsRunLibComTests.c
    ${IOCTESTS}
  )
  target_link_libraries(iocTestHarness ComStatic)

endif(HOST)
