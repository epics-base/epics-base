/* devEventSoft.c */
/* base/src/dev $Id$ */
/*
 *      Author:		Janet Anderson
 *      Date:   	04-21-91
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
 * .03  10-10-92        jba     replaced code with recGblGetLinkValue call
 * .04  03-03-94	mrk	Move constant link value to val only if val is zero
*/
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>

#include	"alarm.h"
#include	"dbDefs.h"
#include	"dbAccess.h"
#include	"recGbl.h"
#include        "recSup.h"
#include	"devSup.h"
#include	"eventRecord.h"

/* Create the dset for devEventSoft */
static long init_record();
static long read_event();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_event;
}devEventSoft={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_event
};

static long init_record(pevent)
    struct eventRecord	*pevent;
{

    /* event.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pevent->inp.type) {
    case (CONSTANT) :
        if(recGblInitConstantLink(&pevent->inp,DBF_USHORT,&pevent->val))
            pevent->udf = FALSE;
        break;
    case (PV_LINK) :
    case (DB_LINK) :
    case (CA_LINK) :
        break;
    default :
	recGblRecordError(S_db_badField,(void *)pevent,
	    "devEventSoft (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_event(pevent)
    struct eventRecord	*pevent;
{
    long status;

    status = dbGetLink(&pevent->inp,DBR_USHORT,&pevent->val,0,0);
    if(pevent->inp.type!=CONSTANT && RTN_SUCCESS(status)) pevent->udf=FALSE;
    return(status);
}
