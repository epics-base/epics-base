/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devBoMpv902.c */
/* base/src/dev $Id$ */

/* devBoMpv902.c - Device Support Routines for  Burr Brown MPV 902  Binary output */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
 */


#include	<vxWorks.h>
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<module_types.h>
#include	<boRecord.h>


/* Create the dset for devAiBoMpv902 */
static long init_record();
static long write_bo();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_bo;
}devBoMpv902={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	write_bo};

static long init_record(pbo)
    struct boRecord	*pbo;
{
    int status=0;
    struct vmeio *pvmeio;
    unsigned int value,mask;

    /* bo.out must be an VME_IO */
    switch (pbo->out.type) {
    case (VME_IO) :
        pvmeio = (struct vmeio *)(&pbo->out.value);
	pbo->mask = 1;
	pbo->mask <<= pvmeio->signal;
        /* read the value via bo driver */
	mask = pbo->mask;
        status = bb902_read(pvmeio->card,mask,&value);
	if(status == 0) pbo->rbv = pbo->rval = value;
	else status = 2;
	break;
    default :
	status = S_db_badField;
	recGblRecordError(status,(void *)pbo,
		"devBoMpv902 (init_record) Illegal OUT field");
    }
    return(status);
}

static long write_bo(pbo)
    struct boRecord	*pbo;
{
	struct vmeio *pvmeio;
	int	    status;
	unsigned int	value,mask;

	
	pvmeio = (struct vmeio *)&(pbo->out.value);
	value = pbo->rval;
	mask = pbo->mask;
	status = bb902_driver(pvmeio->card,value,mask);
	if(status==0) {
		 status = bb902_read(pvmeio->card,mask,&value);
		if(status==0) pbo->rbv = value;
		else{
			if(recGblSetSevr(pbo,READ_ALARM,INVALID_ALARM) && errVerbose
			&& (pbo->stat!=READ_ALARM || pbo->sevr!=INVALID_ALARM))
				recGblRecordError(-1,(void *)pbo,"bb902_read Error");
		}
	} else {
                if(recGblSetSevr(pbo,WRITE_ALARM,INVALID_ALARM) && errVerbose
		&& (pbo->stat!=WRITE_ALARM || pbo->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pbo,"bb902_driver Error");
	}
	return(0);
}
