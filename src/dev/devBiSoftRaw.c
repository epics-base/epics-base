/* devBiSoftRaw.c */
/* share/src/dev $Id$ */

/* devBiSoftRaw.c - Device Support Routines for  Soft Binary Input*/
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
 * .02  02-05-92	jba	Changed function arguments from paddr to precord 
 * .02	03-13-92	jba	ANSI C changes
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
#include	<biRecord.h>
/* Added for Channel Access Links */
long dbCaAddInlink();
long dbCaGetLink();

/* Create the dset for devBiSoftRaw */
long init_record();
long read_bi();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_bi;
}devBiSoftRaw={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_bi};


static long init_record(pbi)
    struct biRecord	*pbi;
{
    long status;

    /* bi.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pbi->inp.type) {
    case (CONSTANT) :
        pbi->rval = pbi->inp.value.value;
        break;
    case (PV_LINK) :
        status = dbCaAddInlink(&(pbi->inp), (void *) pbi, "RVAL");
        if(status) return(status);
        break;
    case (DB_LINK) :
        break;
    case (CA_LINK) :
        break;
    default :
	recGblRecordError(S_db_badField,(void *)pbi,
		"devBiSoftRaw (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_bi(pbi)
    struct biRecord	*pbi;
{
    long status,options,nRequest;

    /* bi.inp must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (pbi->inp.type) {
    case (CONSTANT) :
        break;
    case (DB_LINK) :
        options=0;
        nRequest=1;
        status = dbGetLink(&(pbi->inp.value.db_link),(struct dbCommon *)pbi,DBR_ULONG,
                &pbi->rval,&options,&nRequest);
        if(status!=0) {
                recGblSetSevr(pbi,LINK_ALARM,INVALID_ALARM);
        }
        break;
    case (CA_LINK) :
        if (dbCaGetLink(&(pbi->inp)))
        {
            recGblSetSevr(pbi,LINK_ALARM,INVALID_ALARM);
        } 
        else 
            pbi->udf = FALSE;
        break;
    default :
        if(recGblSetSevr(pbi,SOFT_ALARM,INVALID_ALARM)){
                if(pbi->stat!=SOFT_ALARM) {
                        recGblRecordError(S_db_badField,(void *)pbi,
			    "devBiSoftRaw (read_bi) Illegal INP field");
                }
        }
    }
    return(0);
}
