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
 * double for DBR_FLOAT and DBR_DOUBLE
 * const char* for DBR_STRING
 *
 * eg.
 * testdbPutFieldOk("pvname", DBF_ULONG, (unsigned int)5);
 * testdbPutFieldOk("pvname", DBF_FLOAT, (double)4.1);
 * testdbPutFieldOk("pvname", DBF_STRING, "hello world");
 */
epicsShareFunc void testdbPutFieldOk(const char* pv, short dbrType, ...);
/* Tests for put failure */
epicsShareFunc void testdbPutFieldFail(long status, const char* pv, short dbrType, ...);

epicsShareFunc long testdbVPutField(const char* pv, short dbrType, va_list ap);

epicsShareFunc void testdbGetFieldEqual(const char* pv, short dbrType, ...);
epicsShareFunc void testdbVGetFieldEqual(const char* pv, short dbrType, va_list ap);

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

#ifdef __cplusplus
}
#endif

#endif // EPICSUNITTESTDB_H
