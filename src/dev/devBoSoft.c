/* devBoSoft.c */
/* share/src/dev $Id$ */

/* devBoSoft.c - Device Support Routines for  Soft Binary Output*/
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
 * .03  04-01-92        jba     Changed return of init_record to dont convert
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

/* Create the dset for devBoSoft */
long write_bo();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_bo;
}devBoSoft={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	write_bo};

static long init_record(pbo)
struct boRecord *pbo;
{
 
   long status=0;
 
    if (pbo->out.type == PV_LINK)
        status = dbCaAddOutlink(&(pbo->out), (void *) pbo, "VAL");
 
    /* dont convert */
    if (status == 0 ) status=2;

    return status;
 
} /* end init_record() */

static long write_bo(pbo)
    struct boRecord	*pbo;
{
    long status,options,nrequest;

    /* bo.out must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (pbo->out.type) {
    case (CONSTANT) :
        break;
    case (DB_LINK) :
        status = dbPutLink(&pbo->out.value.db_link,(struct dbCommon *)pbo,DBR_USHORT,&pbo->val,1L);
        if(status!=0) {
                recGblSetSevr(pbo,LINK_ALARM,INVALID_ALARM);
        }
        break;
    case (CA_LINK) :
        options = 0L;
        nrequest = 1L;
        status = dbCaPutLink(&(pbo->out), &options, &nrequest);
        break;
    default :
        if(recGblSetSevr(pbo,SOFT_ALARM,INVALID_ALARM)){
                if(pbo->stat!=SOFT_ALARM) {
                        recGblRecordError(S_db_badField,(void *)pbo,
			    "devBoSoft (write_bo) Illegal OUT field");
                }
        }
    }
    return(0);
}
