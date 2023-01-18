/*************************************************************************\
* Copyright (c) 2013 Brookhaven National Laboratory.
* Copyright (c) 2013 ITER Organization.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

/** @file dbUnitTest.h
 * @brief Helpers for unittests of process database
 * @author Michael Davidsaver, Ralph Lange
 *
 * @see @ref dbunittest
 */

#ifndef EPICSUNITTESTDB_H
#define EPICSUNITTESTDB_H

#include <stdarg.h>

#include "epicsUnitTest.h"
#include "dbAddr.h"
#include "dbCommon.h"

#include "dbCoreAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

/** First step in test database setup
 *
 * @see @ref dbtestskel
 */
DBCORE_API void testdbPrepare(void);
/** Read .dbd or .db file
 *
 * @see @ref dbtestskel
 */
DBCORE_API void testdbReadDatabase(const char* file,
                                       const char* path,
                                       const char* substitutions);
/** Assert success of iocInit()
 *
 * @see @ref dbtestskel
 */
DBCORE_API void testIocInitOk(void);
/** Shutdown test database processing.
 *
 * eg. Stops scan threads
 *
 * @see @ref dbtestskel
 */
DBCORE_API void testIocShutdownOk(void);
/** Final step in test database cleanup
 *
 * @see @ref dbtestskel
 */
DBCORE_API void testdbCleanup(void);

/** Assert that a dbPutField() scalar operation will complete successfully.
 *
 * @code
 * testdbPutFieldOk("some.TPRO", DBF_LONG, 1);
 * @endcode
 *
 * @see @ref dbtestactions
 */
DBCORE_API void testdbPutFieldOk(const char* pv, int dbrType, ...);

/** Assert that a dbPutField() operation will fail with a certain S_\* code
 *
 * @see @ref dbtestactions
 */
DBCORE_API void testdbPutFieldFail(long status, const char* pv, int dbrType, ...);

/** Assert that a dbPutField() scalar operation will complete successfully.
 *
 * @see @ref dbtestactions
 */
DBCORE_API long testdbVPutField(const char* pv, short dbrType, va_list ap);

/** Assert that a dbGetField() scalar operation will complete successfully, with the provided value.
 *
 * @code
 * testdbGetFieldEqual("some.TPRO", DBF_LONG, 0);
 * @endcode
 *
 * @see @ref dbtestactions
 */
DBCORE_API void testdbGetFieldEqual(const char* pv, int dbrType, ...);

/** Assert that a dbGetField() scalar operation will complete successfully, with the provided value.
 *
 * @see @ref dbtestactions
 */
DBCORE_API void testdbVGetFieldEqual(const char* pv, short dbrType, va_list ap);

/** Assert that a dbPutField() array operation will complete successfully.
 *
 * @param pv a PV name, possibly including filter expression
 * @param dbrType a DBF_\* type code (cf. dbfType in dbFldTypes.h)
 * @param count Number of elements in pbuf array
 * @param pbuf Array of values to write
 *
 * @code
 * static const epicsUInt32 putval[] = {1,2,3};
 * testdbPutArrFieldOk("some:wf", DBF_ULONG, NELEMENTS(putval), putval);
 * @endcode
 *
 * @see @ref dbtestactions
 */
DBCORE_API void testdbPutArrFieldOk(const char* pv, short dbrType, unsigned long count, const void *pbuf);

/**
 * @param pv PV name string
 * @param dbfType One of the DBF_\* macros from dbAccess.h
 * @param nRequest Number of elements to request from pv
 * @param pbufcnt  Number of elements pointed to be pbuf
 * @param pbuf     Expected value buffer
 *
 * Execute dbGet() of nRequest elements and compare the result with
 * pbuf (pbufcnt is an element count).
 * Element size is derived from dbfType.
 *
 * nRequest > pbufcnt will detect truncation.
 * nRequest < pbufcnt always fails.
 * nRequest ==pbufcnt checks prefix (actual may be longer than expected)
 */
DBCORE_API void testdbGetArrFieldEqual(const char* pv, short dbfType, long nRequest, unsigned long pbufcnt, const void *pbuf);

/** Obtain pointer to record.
 *
 * Calls testAbort() on failure.  Will never return NULL.
 *
 * @note Remember to dbScanLock() when accessing mutable fields.
 */
DBCORE_API dbCommon* testdbRecordPtr(const char* pv);

typedef struct testMonitor testMonitor;

/** Setup monitoring the named PV for changes */
DBCORE_API testMonitor* testMonitorCreate(const char* pvname, unsigned dbe_mask, unsigned opt);
/** Stop monitoring */
DBCORE_API void testMonitorDestroy(testMonitor*);
/** Return immediately if it has been updated since create, last wait,
 * or reset (count w/ reset=1).
 * Otherwise, block until the value of the target PV is updated.
 */
DBCORE_API void testMonitorWait(testMonitor*);
/** Return the number of monitor events which have occured since create,
 * or a previous reset (called reset=1).
 * Calling w/ reset=0 only returns the count.
 * Calling w/ reset=1 resets the count to zero and ensures that the next
 * wait will block unless subsequent events occur.  Returns the previous
 * count.
 */
DBCORE_API unsigned testMonitorCount(testMonitor*, unsigned reset);

/** Synchronize the shared callback queues.
 *
 * Block until all callback queue jobs which were queued, or running,
 * have completed.
 */
DBCORE_API void testSyncCallback(void);

/** Lock Global convenience mutex for use by test code.
 *
 * @see @ref dbtestmutex
 */
DBCORE_API void testGlobalLock(void);

/** Unlock Global convenience mutex for use by test code.
 *
 * @see @ref dbtestmutex
 */
DBCORE_API void testGlobalUnlock(void);

#ifdef __cplusplus
}
#endif

/** @page dbunittest Unit testing of record processing
 *
 * @see @ref epicsUnitTest.h
 *
 * @section dbtestskel Test skeleton
 *
 * For the impatient, the skeleton of a test:
 *
 * @code
 * #include <dbUnitTest.h>
 * #include <testMain.h>
 *
 * int mytest_registerRecordDeviceDriver(DBBASE *pbase);
 * void testCase(void) {
 *     testdbPrepare();
 *     testdbReadDatabase("mytest.dbd", 0, 0);
 *     mytest_registerRecordDeviceDriver(pdbbase);
 *     testdbReadDatabase("some.db", 0, "VAR=value");
 *     testIocInitOk();
 *     // database running ...
 *     testIocShutdownOk();
 *     testdbCleanup();
 * }
 *
 * MAIN(mytestmain) {
 *     testPlan(0); // adjust number of tests
 *     testCase();
 *     testCase(); // may be repeated if desirable.
 *     return testDone();
 * }
 * @endcode
 *
 * @code
 * TOP = ..
 * include $(TOP)/configure/CONFIG
 *
 * TARGETS += $(COMMON_DIR)/mytest.dbd
 * DBDDEPENDS_FILES += mytest.dbd$(DEP)
 * TESTFILES += $(COMMON_DIR)/mytest.dbd
 * mytest_DBD += base.dbd
 * mytest_DBD += someother.dbd
 *
 * TESTPROD_HOST += mytest
 * mytest_SRCS += mytestmain.c # see above
 * mytest_SRCS += mytest_registerRecordDeviceDriver.cpp
 * TESTFILES += some.db
 *
 * include $(TOP)/configure/RULES
 * @endcode
 *
 * @section dbtestactions Actions
 *
 * Several helper functions are provided to interact with a running database.
 *
 * @li testdbPutFieldOk()
 * @li testdbPutFieldFail()
 * @li testdbPutArrFieldOk()
 * @li testdbVPutField()
 * @li testdbGetFieldEqual()
 * @li testdbVGetFieldEqual()
 *
 * Correct argument types must be used with var-arg functions.
 *
 * @li int for DBR_UCHAR, DBR_CHAR, DBR_USHORT, DBR_SHORT, DBR_LONG
 * @li unsigned int for DBR_ULONG
 * @li long long for DBF_INT64
 * @li unsigned long long for DBF_UINT64
 * @li double for DBR_FLOAT and DBR_DOUBLE
 * @li const char* for DBR_STRING
 *
 * @see enum dbfType in dbFldTypes.h
 *
 * @code
 * testdbPutFieldOk("pvname", DBF_ULONG, (unsigned int)5);
 * testdbPutFieldOk("pvname", DBF_FLOAT, (double)4.1);
 * testdbPutFieldOk("pvname", DBF_STRING, "hello world");
 * @endcode
 *
 * @section dbtestmonitor Monitoring for changes
 *
 * When Put and Get aren't sufficient, testMonitor may help to setup and monitor for changes.
 *
 * @li testMonitorCreate()
 * @li testMonitorDestroy()
 * @li testMonitorWait()
 * @li testMonitorCount()
 *
 * @section dbtestsync Synchronizing
 *
 * Helpers to synchronize with some database worker threads
 *
 * @li testSyncCallback()
 *
 * @section dbtestmutex Global mutex for use by test code.
 *
 * This utility mutex is intended to be used to avoid races in situations
 * where some other synchronization primitive is being destroyed (epicsEvent,
 * epicsMutex, ...) and use of epicsThreadMustJoin() is impractical.
 *
 * For example.  The following has a subtle race where the event may be
 * destroyed (free()'d) before the call to epicsEventMustSignal() has
 * returned.  On some targets this leads to a use after free() error.
 *
 * @code
 * epicsEventId evt;
 * void thread1() {
 *   evt = epicsEventMustCreate(...);
 *   // spawn thread2()
 *   epicsEventMustWait(evt);
 *   epicsEventDestroy(evt); // <- Racer
 * }
 * // ...
 * void thread2() {
 *   epicsEventMustSignal(evt); // <- Racer
 * }
 * @endcode
 *
 * When possible, the best way to avoid this race would be to join the worker
 * before destroying the event.
 *
 * @code
 * epicsEventId evt;
 * void thread1() {
 *     epicsThreadOpts opts = EPICS_THREAD_OPTS_INIT;
 *     epicsThreadId t2;
 *     opts.joinable = 1;
 *     evt = epicsEventMustCreate(...);
 *     t2 = epicsThreadCreateOpt("thread2", &thread2, NULL, &opts);
 *     assert(t2);
 *     epicsEventMustWait(evt);
 *     epicsThreadMustJoin(t2);
 *     epicsEventDestroy(evt);
 * }
 * void thread2() {
 *   epicsEventMustSignal(evt);
 * }
 * @endcode
 *
 * Another way to avoid this race is to use a global mutex to ensure
 * that epicsEventMustSignal() has returned before destroying the event.
 * testGlobalLock() and testGlobalUnlock() provide access to such a mutex.
 *
 * @code
 * epicsEventId evt;
 * void thread1() {
 *   evt = epicsEventMustCreate(...);
 *   // spawn thread2()
 *   epicsEventMustWait(evt);
 *   testGlobalLock();   // <-- added
 *   epicsEventDestroy(evt);
 *   testGlobalUnlock(); // <-- added
 * }
 * // ...
 * void thread2() {
 *   testGlobalLock();   // <-- added
 *   epicsEventMustSignal(evt);
 *   testGlobalUnlock(); // <-- added
 * }
 * @endcode
 *
 * This must be a global mutex to avoid simply shifting the race
 * from the event to a locally allocated mutex.
 */

#endif // EPICSUNITTESTDB_H
