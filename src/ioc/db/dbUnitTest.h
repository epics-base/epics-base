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
epicsShareFunc void testdbPutFieldOk(const char* pv, int dbrType, ...);
/* Tests for put failure */
epicsShareFunc void testdbPutFieldFail(long status, const char* pv, int dbrType, ...);

epicsShareFunc long testdbVPutField(const char* pv, short dbrType, va_list ap);

epicsShareFunc void testdbGetFieldEqual(const char* pv, int dbrType, ...);
epicsShareFunc void testdbVGetFieldEqual(const char* pv, short dbrType, va_list ap);

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

#ifdef __cplusplus
}
#endif

#endif // EPICSUNITTESTDB_H
