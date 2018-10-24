/*************************************************************************\
* Copyright (c) 2013 Brookhaven National Laboratory.
* Copyright (c) 2013 ITER Organization.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

/*
 *  Author: Michael Davidsaver <mdavidsaver@bnl.gov>
 *          Ralph Lange <Ralph.Lange@gmx.de>
 */

#ifndef EPICSUNITTESTDB_H
#define EPICSUNITTESTDB_H

#include <stdarg.h>

#include "epicsUnitTest.h"
#include "dbAddr.h"
#include "dbCommon.h"

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc void testdbPrepare(void);
epicsShareFunc void testdbReadDatabase(const char* file,
                                       const char* path,
                                       const char* substitutions);
epicsShareFunc void testIocInitOk(void);
epicsShareFunc void testIocShutdownOk(void);
epicsShareFunc void testdbCleanup(void);

/* Correct argument types must be used with this var-arg function!
 * Doing otherwise will result in corruption of argument values!
 *
 * int for DBR_UCHAR, DBR_CHAR, DBR_USHORT, DBR_SHORT, DBR_LONG
 * unsigned int for DBR_ULONG
 * long long for DBF_INT64
 * unsigned long long for DBF_UINT64
 * double for DBR_FLOAT and DBR_DOUBLE
 * const char* for DBR_STRING
 *
 * eg.
 * testdbPutFieldOk("pvname", DBF_ULONG, (unsigned int)5);
 * testdbPutFieldOk("pvname", DBF_FLOAT, (double)4.1);
 * testdbPutFieldOk("pvname", DBF_STRING, "hello world");
 */
epicsShareFunc void testdbPutFieldOk(const char* pv, int dbrType, ...);
/* Tests for put failure */
epicsShareFunc void testdbPutFieldFail(long status, const char* pv, int dbrType, ...);

epicsShareFunc long testdbVPutField(const char* pv, short dbrType, va_list ap);

epicsShareFunc void testdbGetFieldEqual(const char* pv, int dbrType, ...);
epicsShareFunc void testdbVGetFieldEqual(const char* pv, short dbrType, va_list ap);

epicsShareFunc void testdbPutArrFieldOk(const char* pv, short dbrType, unsigned long count, const void *pbuf);

/**
 * @param pv PV name string
 * @param dbfType One of the DBF_* macros from dbAccess.h
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
epicsShareFunc void testdbGetArrFieldEqual(const char* pv, short dbfType, long nRequest, unsigned long pbufcnt, const void *pbuf);

epicsShareFunc dbCommon* testdbRecordPtr(const char* pv);

typedef struct testMonitor testMonitor;

/* Begin monitoring the named PV for changes */
epicsShareFunc testMonitor* testMonitorCreate(const char* pvname, unsigned dbe_mask, unsigned opt);
/* End monitoring */
epicsShareFunc void testMonitorDestroy(testMonitor*);
/* Return immediately if it has been updated since create, last wait,
 * or reset (count w/ reset=1).
 * Otherwise, block until the value of the target PV is updated.
 */
epicsShareFunc void testMonitorWait(testMonitor*);
/* Return the number of monitor events which have occured since create,
 * or a pervious reset (called reset=1).
 * Calling w/ reset=0 only returns the count.
 * Calling w/ reset=1 resets the count to zero and ensures that the next
 * wait will block unless subsequent events occur.  Returns the previous
 * count.
 */
epicsShareFunc unsigned testMonitorCount(testMonitor*, unsigned reset);

/** Synchronize the shared callback queues.
 *
 * Block until all callback queue jobs which were queued, or running,
 * have completed.
 */
epicsShareFunc void testSyncCallback(void);

/** Global mutex for use by test code.
 *
 * This utility mutex is intended to be used to avoid races in situations
 * where some other syncronization primitive is being destroyed (epicsEvent,
 * epicsMutex, ...).
 *
 * For example.  The following has a subtle race where the event may be
 * destroyed (free()'d) before the call to epicsEventMustSignal() has
 * returned.  On some targets this leads to a use after free() error.
 *
 @code
 epicsEventId evt;
 void thread1() {
   evt = epicsEventMustCreate(...);
   // spawn thread2()
   epicsEventMustWait(evt);
   epicsEventDestroy(evt);
 }
 // ...
 void thread2() {
   epicsEventMustSignal(evt);
 }
 @endcode
 *
 * One way to avoid this race is to use a global mutex to ensure
 * that epicsEventMustSignal() has returned before destroying
 * the event.
 *
 @code
 epicsEventId evt;
 void thread1() {
   evt = epicsEventMustCreate(...);
   // spawn thread2()
   epicsEventMustWait(evt);
   testGlobalLock();   // <-- added
   epicsEventDestroy(evt);
   testGlobalUnlock(); // <-- added
 }
 // ...
 void thread2() {
   testGlobalLock();   // <-- added
   epicsEventMustSignal(evt);
   testGlobalUnlock(); // <-- added
 }
 @endcode
 *
 * This must be a global mutex to avoid simply shifting the race
 * from the event to a locally allocated mutex.
 */
epicsShareFunc void testGlobalLock(void);
epicsShareFunc void testGlobalUnlock(void);

#ifdef __cplusplus
}
#endif

#endif // EPICSUNITTESTDB_H
