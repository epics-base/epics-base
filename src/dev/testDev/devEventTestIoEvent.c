/* devEventTestIoEvent.c */
/* base/src/dev $Id$ */
/*
 *      Author:  	Marty Kraimer
 *      Date:           01/09/92
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
 * .00  12-13-91        jba     Initial definition
 * .02	03-13-92	jba	ANSI C changes
 *      ...
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "callback.h"
#include "dbScan.h"
#include "recSup.h"
#include "devSup.h"
#include "eventRecord.h"
/* Create the dset for devEventTestIoEvent */
static long init_record();
static long get_ioint_info();
static long read_event();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_event;
}devEventTestIoEvent={
	5,
	NULL,
	NULL,
	init_record,
	get_ioint_info,
	read_event
};

typedef struct myCallback {
    CALLBACK callback;
    IOSCANPVT ioscanpvt;
}myCallback;

static void myCallbackFunc(CALLBACK *arg)
{
    myCallback *pcallback;

    callbackGetUser(pcallback,arg);
    scanIoRequest(pcallback->ioscanpvt);
}


static long init_record(pevent,pass)
    eventRecord *pevent;
    int pass;
{
    myCallback *pcallback;

    pcallback = (myCallback *)(calloc(1,sizeof(myCallback)));
    scanIoInit(&pcallback->ioscanpvt);
    callbackSetCallback(myCallbackFunc,&pcallback->callback);
    callbackSetUser(pcallback,&pcallback->callback);
    pevent->dpvt = (void *)pcallback;
    return(0);
}

static long get_ioint_info(
	int 			cmd,
	struct eventRecord 	*pevent,
	IOSCANPVT		*ppvt)
{
    myCallback *pcallback = (myCallback *)pevent->dpvt;

    *ppvt = pcallback->ioscanpvt;
    return(0);
}
    
static long read_event(pevent)
    struct eventRecord	*pevent;
{
    myCallback *pcallback= (myCallback *)pevent->dpvt;

    if(pevent->proc<=0) return(0);
    pevent->udf = FALSE;
    printf("%s Requesting Next ioEnevt\n",pevent->name);
    callbackRequestDelayed(&pcallback->callback,(double)pevent->proc);
    return(0);
}
