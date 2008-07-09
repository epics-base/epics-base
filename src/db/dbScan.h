/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$
 *
 *      Author:         Marty Kraimer
 *      Date:           07-17-91
 */

#ifndef INCdbScanH
#define INCdbScanH

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Note that these must match the first three definitions in choiceGbl.ascii*/
#define SCAN_PASSIVE		0
#define SCAN_EVENT		1
#define SCAN_IO_EVENT		2
#define SCAN_1ST_PERIODIC	3

/*definitions for SCAN_IO_EVENT */
struct io_scan_list;

typedef struct io_scan_list *IOSCANPVT;

struct dbCommon;

epicsShareFunc long scanInit(void);
epicsShareFunc void scanRun(void);
epicsShareFunc void scanPause(void);

epicsShareFunc void post_event(int event);
epicsShareFunc void scanAdd(struct dbCommon *);
epicsShareFunc void scanDelete(struct dbCommon *);
epicsShareFunc double scanPeriod(int scan);
epicsShareFunc void scanOnce(struct dbCommon *);
epicsShareFunc int scanOnceSetQueueSize(int size);

/*print periodic lists*/
epicsShareFunc int scanppl(double rate);

/*print event lists*/
epicsShareFunc int scanpel(int event_number);

/*print io_event list*/
epicsShareFunc int scanpiol(void);

epicsShareFunc void scanIoInit(IOSCANPVT *);
epicsShareFunc void scanIoRequest(IOSCANPVT);

#ifdef __cplusplus
}
#endif

#endif
