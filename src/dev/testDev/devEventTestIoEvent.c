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

#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<wdLib.h>
#include	<string.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<dbScan.h>
#include	<recSup.h>
#include	<devSup.h>
#include	<eventRecord.h>
/* Create the dset for devEventTestIoEvent */
static long init();
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
	init,
	NULL,
	get_ioint_info,
	read_event
};

static IOSCANPVT ioscanpvt;
WDOG_ID wd_id=NULL;

static long init(after)
int after;
{
    if(after) return(0);
    scanIoInit(&ioscanpvt);
    return(0);
}


static long get_ioint_info(
	int 			cmd,
	struct eventRecord 	*pr,
	IOSCANPVT		*ppvt)
{

    *ppvt = ioscanpvt;
    return(0);
}

static long read_event(pevent)
    struct eventRecord	*pevent;
{
	int	wait_time;

	wait_time = (int)(pevent->proc * vxTicksPerSecond);
	if(wait_time<=0) return(0);
	pevent->udf = FALSE;
	if(wd_id==NULL) wd_id = wdCreate();
	printf("%s Requesting Next ioEnevt\n",pevent->name);
	wdStart(wd_id,wait_time,(FUNCPTR)scanIoRequest,(int)ioscanpvt);
	return(0);
}
