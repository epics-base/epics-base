/* devBoMpv902.c */
/* share/src/dev $Id$ */

/* devBoMpv902.c - Device Support Routines for  Burr Brown MPV 902  Binary output */
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
#include	<types.h>
#include	<stdioLib.h>
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
