/* $Id$
 *
 *      Author:         Marty Kraimer
 *      Date:           07-17-91
 */

/********************COPYRIGHT NOTIFICATION**********************************
This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
****************************************************************************/

#ifndef INCdbScanh
#define INCdbScanh 1

#include "shareLib.h"

/* Note that these must match the first three definitions in choiceGbl.ascii*/
#define SCAN_PASSIVE		0
#define SCAN_EVENT		1
#define SCAN_IO_EVENT		2
#define SCAN_1ST_PERIODIC	3

/*definitions for SCAN_IO_EVENT */
typedef void * IOSCANPVT;
extern volatile int interruptAccept;

epicsShareFunc long epicsShareAPI scanInit(void);
epicsShareFunc void epicsShareAPI post_event(int event);
epicsShareFunc void epicsShareAPI scanAdd(struct dbCommon *);
epicsShareFunc void epicsShareAPI scanDelete(struct dbCommon *);
epicsShareFunc void epicsShareAPI scanOnce(void *precord);
epicsShareFunc int epicsShareAPI scanOnceSetQueueSize(int size);
/*print periodic lists*/
epicsShareFunc int epicsShareAPI scanppl(double rate);
/*print event lists*/
epicsShareFunc int epicsShareAPI scanpel(int event_number);
/*print io_event list*/
epicsShareFunc int epicsShareAPI scanpiol(void);
epicsShareFunc void epicsShareAPI scanIoInit(IOSCANPVT *);
epicsShareFunc void epicsShareAPI scanIoRequest(IOSCANPVT);

#endif
