/* $Id$
 *
 *      Author:         Marty Kraimer
 *      Date:           07-17-91
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  mm-dd-yy        mrk	Initial version
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
void scanOnce(void *precord);
int scanOnceSetQueueSize(int size);
int scanppl(void);			/*print periodic lists*/
int scanpel(void);			/*print event lists*/
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
