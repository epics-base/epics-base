/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$
 *
 *      Author:         Marty Kraimer
 *      Date:           07-17-91
 */

#ifndef INCdbScanh
#define INCdbScanh 1
#include <ellLib.h>
#include <dbAccess.h>
#include <callback.h>
/* Note that these must match the first three definitions in choiceGbl.ascii*/
#define SCAN_PASSIVE		0
#define SCAN_EVENT		1
#define SCAN_IO_EVENT		2
#define SCAN_1ST_PERIODIC	3

/*definitions for SCAN_IO_EVENT */
typedef void * IOSCANPVT;
extern volatile int interruptAccept;

#ifdef __STDC__
long scanInit(void);
void post_event(int event);
void scanAdd(struct dbCommon *);
void scanDelete(struct dbCommon *);
double scanPeriod(int scan);
void scanOnce(void *precord);
int scanOnceSetQueueSize(int size);
int scanppl(double rate);		/*print periodic lists*/
int scanpel(int event_number);		/*print event lists*/
int scanpiol(void);			/*print io_event list*/
void scanIoInit(IOSCANPVT *);
void scanIoRequest(IOSCANPVT);

#else
long scanInit();
void post_event();
void scanAdd();
void scanDelete();
void scanppl();
void scanpel();
void scanpiol();
void scanIoInit();
void scanIoRequest();
#endif /*__STDC__*/
#endif
