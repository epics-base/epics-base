/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devEventTestIoEvent.c */
/* base/src/dev $Id$ */
/*
 *      Author:  	Marty Kraimer
 *      Date:           01/09/92
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
