/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devAoVmiVme4100.c.c */
/* base/src/dev $Id$ */

/* Device Support Routines for VMI4100 analog output*/
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
#include	<special.h>
#include	<module_types.h>
#include	<aoRecord.h>

/* The following must match the definition in choiceGbl.ascii */
#define LINEAR 1

/* Create the dset for devAoAoVmiVme4100 */
static long init_record();
static long write_ao();
static long special_linconv();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_ao;
	DEVSUPFUN	special_linconv;
}devAoVmiVme4100={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	write_ao,
	special_linconv};



static long init_record(pao)
    struct aoRecord	*pao;
{

    /* ao.out must be an VME_IO */
    switch (pao->out.type) {
    case (VME_IO) :
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pao,
		"devAoVmiVme4100 (init_record) Illegal OUT field");
	return(S_db_badField);
    }

    /* set linear conversion slope*/
    pao->eslo = (pao->eguf -pao->egul)/4095.0;

    /* It is not possible to read current value of card */
    /* Tell recSup not to convert			*/
    return(2);
}

static long write_ao(pao)
    struct aoRecord	*pao;
{
	struct vmeio 	*pvmeio;
	int	    	status;
	unsigned short  value,rbvalue;

	
	pvmeio = (struct vmeio *)&(pao->out.value);
	value = pao->rval;
	status = vmi4100_driver(pvmeio->card,pvmeio->signal,&value,&rbvalue);
	if(status==0 || status==-2) pao->rbv = rbvalue;
	if(status==-1) {
		status = 0;
                if(recGblSetSevr(pao,WRITE_ALARM,INVALID_ALARM) && errVerbose
		&& (pao->stat!=WRITE_ALARM || pao->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pao,"vmi4100_driver Error");
	}else if(status==-2) {
		status=0;
                recGblSetSevr(pao,HW_LIMIT_ALARM,INVALID_ALARM);
	}
	return(status);
}


static long special_linconv(pao,after)
    struct aoRecord	*pao;
    int after;
{

    if(!after) return(0);
    /* set linear conversion slope*/
    pao->eslo = (pao->eguf -pao->egul)/4095.0;
    return(0);
}


