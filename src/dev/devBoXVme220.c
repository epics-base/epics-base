/* devBoXVme220.c */
/* share/src/dev $Id$ */

/* devBoXVme220.c - Device Support Routines for XYcom 32 bit binary output*/
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
 *      ...
 */


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>

#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<module_types.h>
#include	<boRecord.h>


/* Create the dset for devAiBoXVme220 */
long init_record();
long write_bo();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_bo;
}devBoXVme220={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	write_bo};

static long init_record(pbo)
    struct boRecord	*pbo;
{
    char message[100];
    unsigned int value;
    int status;
    struct vmeio *pvmeio;

    /* bo.out must be an VME_IO */
    switch (pbo->out.type) {
    case (VME_IO) :
	pvmeio = (struct vmeio *)&(pbo->out.value);
	pbo->mask = 1;
	pbo->mask <<= pvmeio->signal;
        status = bo_read(pvmeio->card,pbo->mask,&value,XY220);
        if(status == 0) {
                pbo->rval = value;
        } else if(status == -1) {
                strcpy(message,pbo->name);
                strcat(message,": devBoXVme220 (init_record) card does not exist");
                errMessage(S_db_badField,message);
                return(S_db_badField);
        } else {
                strcpy(message,pbo->name);
                strcat(message,": devBoXVme220 (init_record) illegal device");
                errMessage(S_db_badField,message);
                return(S_db_badField);
        }
	break;
    default :
	strcpy(message,pbo->name);
	strcat(message,": devBoXVme220 (init_record) Illegal OUT field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long write_bo(pbo)
    struct boRecord	*pbo;
{
	struct vmeio *pvmeio;
	int	    status;

	
	pvmeio = (struct vmeio *)&(pbo->out.value);
	status = bo_driver(pvmeio->card,pbo->rval,pbo->mask,XY220);
	if(status!=0) {
                recGblSetSevr(pbo,WRITE_ALARM,VALID_ALARM);
	}
	return(status);
}
