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
epicsShareFunc int testiocInit(void);
epicsShareFunc int testiocShutdown(void);
epicsShareFunc void testdbCleanup(void);

/* Scalar only version.
 *
 * Remember to use the correct argument type!s
 *
 * int for DBR_UCHAR, DBR_CHAR, DBR_USHORT, DBR_SHORT, DBR_LONG
 * unsigned int for DBR_ULONG
 * double for DBR_FLOAT and DBR_DOUBLE
 * const char* for DBR_STRING
 */
epicsShareFunc long testdbPutField(const char* pv, short dbrType, ...);
epicsShareFunc long testVdbPutField(const char* pv, short dbrType, va_list ap);

epicsShareFunc dbCommon* testGetRecord(const char* pv);

#ifdef __cplusplus
}
#endif

#endif // EPICSUNITTESTDB_H
