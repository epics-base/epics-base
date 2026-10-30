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

/** @brief Lookup, or create, named Event record list
 * @param event Name
 * @return Record list handle, or NULL on allocation failure.
 *
 * @since 3.15.0.1
 */
DBCORE_API EVENTPVT eventNameToHandle(const char* event);
/** @brief Request scan of Event record list
 * @param epvt Event list handle
 *
 * Queue request for callback worker thread(s) to scan records.
 * Lookup named event list with eventNameToHandle() .
 *
 * @since 3.15.0.1
 */
DBCORE_API void postEvent(EVENTPVT epvt);
/** @brief Process numbered Event record list
 * @param event Event number in the range 1 through `NUM_TIME_EVENTS-1` (255) inclusive.
 * @deprecated In favor of postEvent()
 */
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
 *
 * @since 3.16.0.1 Added
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
/** @brief Process all records on the scan list for the specified priority.
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

/** @file dbScan.h
 *
 * Mechanics for interacting with Process Database record scanning
 * utilities.
 *
 * @section dbscanoverflow Scan Queue Overflow
 *
 * Several of the thread backed scanning mechanisms have work
 * queues which may overflow.
 * Well written EPICS drivers should avoid this as described below.
 *
 * @section dbscaniointr I/O Intr scanning
 *
 * An EPICS driver may use scanIoInit() to allocate a scan list.
 * This scan list is associated with a record through dset::get_ioint_info ,
 * and triggered with scanIoRequest() (asynchronous)
 * or scanIoImmediate() (synchronous).
 *
 * scanIoSetComplete() may be used to request notification after
 * any records on the list have begun processing.
 * This may be used to defer future scanIoRequest() until
 * past requests have been satisfied,
 * which avoids possible overflowing of the underlying queue
 * of the worker thread.
 *
 * @section dbscanname Named Event scanning
 *
 * Event names may appear in the `EVNT` field of any record.
 * When SCAN is also set to `Event`, the that record will process
 * each time this Event occurs.
 *
 * An eventRecord provides one way to trigger scanning.
 *
 * eventNameToHandle() may be used by an EPICS driver to lookup/create
 * a process-wide named scan list.
 * Then call postEvent() to trigger scanning.
 *
 * Legacy applications may use post_event() for the special numbered
 * Events (1 through 255).
 * `post_event(1)` is equivalent to `eventNameToHandle("1")`
 * followed by a postEvent().
 *
 * There is no way for a caller of postEvent() to avoid scan
 * queue overflow.
 *
 * @section dbscanonce Once Queue
 *
 * The "once" queue is meant for special cases of one-time
 * processing.  eg. the initial PINI scanning.
 * EPICS drivers may call scanOnceCallback() explicitly,
 * but are encouraged where practical to call dbProcess() from
 * a driver managed worker thread.
 *
 * The process-wide "once" queue has a fixed size, which may be
 * modified by scanOnceSetQueueSize() .
 */

#endif
