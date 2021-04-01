/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 *      Author:         Marty Kraimer
 *      Date:           07-17-91
 */

#ifndef INCdbScanH
#define INCdbScanH

#include <limits.h>

#include "menuScan.h"
#include "dbCoreAPI.h"
#include "compilerDependencies.h"
#include "devSup.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SCAN_PASSIVE        menuScanPassive
#define SCAN_EVENT          menuScanEvent
#define SCAN_IO_EVENT       menuScanI_O_Intr
#define SCAN_1ST_PERIODIC   (menuScanI_O_Intr + 1)

#define MAX_PHASE           SHRT_MAX
#define MIN_PHASE           SHRT_MIN

/*definitions for I/O Interrupt Scanning */
/* IOSCANPVT now defined in devSup.h */
typedef struct event_list *EVENTPVT;

struct dbCommon;

typedef void (*io_scan_complete)(void *usr, IOSCANPVT, int prio);
typedef void (*once_complete)(void *usr, struct dbCommon*);

typedef struct scanOnceQueueStats {
    int size;
    int numUsed;
    int maxUsed;
    int numOverflow;
} scanOnceQueueStats;

DBCORE_API long scanInit(void);
DBCORE_API void scanRun(void);
DBCORE_API void scanPause(void);
DBCORE_API void scanStop(void);
DBCORE_API void scanCleanup(void);

DBCORE_API EVENTPVT eventNameToHandle(const char* event);
DBCORE_API void postEvent(EVENTPVT epvt);
DBCORE_API void post_event(int event);
DBCORE_API void scanAdd(struct dbCommon *);
DBCORE_API void scanDelete(struct dbCommon *);
DBCORE_API double scanPeriod(int scan);
DBCORE_API int scanOnce(struct dbCommon *);
DBCORE_API int scanOnceCallback(struct dbCommon *, once_complete cb, void *usr);
DBCORE_API int scanOnceSetQueueSize(int size);
DBCORE_API int scanOnceQueueStatus(const int reset, scanOnceQueueStats *result);
DBCORE_API void scanOnceQueueShow(const int reset);

/*print periodic lists*/
DBCORE_API int scanppl(double rate);

/*print event lists*/
DBCORE_API int scanpel(const char *event_name);

/*print io_event list*/
DBCORE_API int scanpiol(void);

DBCORE_API void scanIoInit(IOSCANPVT *ppios);
DBCORE_API unsigned int scanIoRequest(IOSCANPVT pios);
DBCORE_API unsigned int scanIoImmediate(IOSCANPVT pios, int prio);
DBCORE_API void scanIoSetComplete(IOSCANPVT, io_scan_complete, void *usr);

#ifdef __cplusplus
}
#endif

#endif
