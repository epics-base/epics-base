/* devAoVmiVme4100.c.c */
/* share/src/dev $Id$ */

/* Device Support Routines for VMI4100 analog output*/
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
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
 * .01  11-11-91        jba     Moved set of alarm stat and sevr to macros
 * .02	03-13-92	jba	ANSI C changes
 * .03	05-11-92	mrk	Remove read at init (device does not support)
 *      ...
 */


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
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
long init_record();
long write_ao();
long special_linconv();

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
	recGblRecordError(S_db_badField,pao,
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
                recGblSetSevr(pao,WRITE_ALARM,VALID_ALARM);
	}else if(status==-2) {
		status=0;
                recGblSetSevr(pao,HW_LIMIT_ALARM,VALID_ALARM);
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


