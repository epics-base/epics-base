/* devBoSoftRaw.c */
/* share/src/dev $Id$ */

/* devBoSoftRaw.c - Device Support Routines for  SoftRaw Binary Output*/
/*
 *      Author:		Janet Anderson
 *      Date:		3-28-92
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
 * .01  03-28-92        jba     Initial version
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

/* added for Channel Access Links */
long dbCaAddOutlink();
long dbCaPutLink();
long init_record();

/* Create the dset for devBoSoftRaw */
long write_bo();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_bo;
}devBoSoftRaw={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	write_bo};



static long init_record(pbo)
struct boRecord *pbo;
{
 
long status;
 
    if (pbo->out.type == PV_LINK)
        status = dbCaAddOutlink(&(pbo->out), (void *) pbo, "RVAL");
    else
        status = 0L;
 
    return status;
 
} /* end init_record() */

static long write_bo(pbo)
    struct boRecord	*pbo;
{
    char message[100];
    long status;
/* added for Channel Access Links */
long options;
long nrequest;

    /* bo.out must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (pbo->out.type) {
    case (CONSTANT) :
        break;
    case (DB_LINK) :
        status = dbPutLink(&pbo->out.value.db_link,(struct dbCommon *)pbo,DBR_ULONG,
                &pbo->rval,1L);
        if(status!=0) {
                recGblSetSevr(pbo,LINK_ALARM,VALID_ALARM);
        }
        break;
    case (CA_LINK) :
        options = 0L;
        nrequest = 1L;
        status = dbCaPutLink(&(pbo->out), &options, &nrequest);
        break;
    default :
        if(recGblSetSevr(pbo,SOFT_ALARM,VALID_ALARM)){
                if(pbo->stat!=SOFT_ALARM) {
                        strcpy(message,pbo->name);
                        strcat(message,": devBoSoftRaw (write_bo) Illegal OUT field");
                        errMessage(S_db_badField,message);
                }
        }
    }
    return(0);
}
