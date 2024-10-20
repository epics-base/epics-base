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
/** Shorthand for scanOnceCallback(prec, NULL, NULL)
 */
DBCORE_API int scanOnce(struct dbCommon *prec);
/** @brief scanOnce Request immediate record processing from another thread.
 *
 * Queue a request for record processing from the dedicated "Once" thread.
 * Request may fail if Once queue overflows.  See scanOnceSetQueueSize()
 *
 * @param prec Record to process
 * @param cb Function called after target record dbProcess()
 *           Does not wait for async record completion.
 * @param usr Argumentfor cb
 * @return Zero on success.  Non-zero if the request could not be queued.
 */
DBCORE_API int scanOnceCallback(struct dbCommon *prec, once_complete cb, void *usr);
/** @brief Set Once queue size
 *
 * Must be called prior to iocInit()
 *
 * @param size New size.  May be smaller
 * @return Zero on success
 */
DBCORE_API int scanOnceSetQueueSize(int size);
DBCORE_API int scanOnceQueueStatus(const int reset, scanOnceQueueStats *result);
DBCORE_API void scanOnceQueueShow(const int reset);

/*print periodic lists*/
DBCORE_API int scanppl(double rate);

/*print event lists*/
DBCORE_API int scanpel(const char *event_name);

/*print io_event list*/
DBCORE_API int scanpiol(void);

/** @brief Initialize "I/O Intr" source
 * @param ppios Pointer to scan list to be initialized
 *
 * Afterwards this IOSCANPVT may be assigned during a get_ioint_info() callback.
 * See typed_dset::get_ioint_info()
 *
 * @note There is currently no way to free this allocation.
 */
DBCORE_API void scanIoInit(IOSCANPVT *ppios);
/** @brief Request processing of all associated records from callback threads
 * @param pios The scan list
 * @pre pios must be initialized by scanIoInit()
 * @return
 */
DBCORE_API unsigned int scanIoRequest(IOSCANPVT pios);
/** @brief Process all records on the scan list for the specificed priority.
 *
 * Also executes the callback set by scanIoSetComplete()
 *
 * @param pios The scan list
 * @param prio one of priorityLow through priorityHigh (defined in callback.h).
 *        A value between 0 and NUM_CALLBACK_PRIORITIES-1 .
 * @return Zero if the scan list was empty or 1<<prio
 * @since 3.16.0.1
 */
DBCORE_API unsigned int scanIoImmediate(IOSCANPVT pios, int prio);
/** @brief Set scan list completion callback
 *
 * Replace the callback which will be invoked after record processing begins.
 * Asynchronous record processing may be ongoing.
 *
 * @since 3.15.0.2
 */
DBCORE_API void scanIoSetComplete(IOSCANPVT, io_scan_complete, void *usr);

#ifdef __cplusplus
}
#endif

#endif
