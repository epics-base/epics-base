/* devMbboMpv902.c */
/* base/src/dev $Id$ */

/* devMbboMpv902.c - Device Support Routines	*/
/* Burr Brown MPV 902 				*/
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
 * .03	02-08-94	mrk	Issue Hardware Errors BUT prevent Error Message Storms
 *      ...
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
#include	<mbboRecord.h>


/* Create the dset for devAiMbboMpv902 */
static long init_record();
static long write_mbbo();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_mbbo;
}devMbboMpv902={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	write_mbbo};

static long init_record(pmbbo)
    struct mbboRecord	*pmbbo;
{
    unsigned int value,mask;
    struct vmeio *pvmeio;
    int		status=0;

    /* mbbo.out must be an VME_IO */
    switch (pmbbo->out.type) {
    case (VME_IO) :
	pvmeio = &(pmbbo->out.value.vmeio);
	pmbbo->shft = pvmeio->signal;
	pmbbo->mask <<= pmbbo->shft;
	mask = pmbbo->mask;
	status = bb902_read(pvmeio->card,mask,&value);
	if(status==0) pmbbo->rbv = pmbbo->rval = value;
	else status = 2;
	break;
    default :
	status = S_db_badField;
	recGblRecordError(status,(void *)pmbbo,
		"devMbboMpv902 (init_record) Illegal OUT field");
    }
    return(status);
}

static long write_mbbo(pmbbo)
    struct mbboRecord	*pmbbo;
{
	struct vmeio *pvmeio;
	int	    status;
	unsigned int value,mask;

	
	pvmeio = &(pmbbo->out.value.vmeio);

	value = pmbbo->rval;
	mask = pmbbo->mask;
	status = bb902_driver(pvmeio->card,value,mask);
	if(status==0) {
		status = bb902_read(pvmeio->card,mask,&value);
		if(status==0) pmbbo->rbv = value;
                else {
			if(recGblSetSevr(pmbbo,READ_ALARM,INVALID_ALARM) && errVerbose
			&& (pmbbo->stat!=READ_ALARM || pmbbo->sevr!=INVALID_ALARM))
				recGblRecordError(-1,(void *)pmbbo,"bb902_read Error");
		}
	} else {
                if(recGblSetSevr(pmbbo,WRITE_ALARM,INVALID_ALARM) && errVerbose
		&& (pmbbo->stat!=WRITE_ALARM || pmbbo->sevr!=INVALID_ALARM))
			recGblRecordError(-1,(void *)pmbbo,"bb902_driver Error");
	}
	return(0);
}
