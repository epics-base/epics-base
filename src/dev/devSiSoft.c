/* devSiSoft.c */
/* base/src/dev $Id$ */

/* devSiSoft.c - Device Support Routines for Soft String Input */
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
 * .01  11-11-91        jba     Moved set of alarm stat and sevr to macros
 * .02	03-13-92	jba	ANSI C changes
 * .03  10-10-92        jba     replaced code with recGblGetLinkValue call
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
#include	<stringinRecord.h>

/* Create the dset for devSiSoft */
static long init_record();
static long read_stringin();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_stringin;
}devSiSoft={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_stringin};

static long init_record(pstringin)
    struct stringinRecord	*pstringin;
{
    long status;

    /* stringin.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pstringin->inp.type) {
    case (CONSTANT) :
	recGblInitConstantLink(&pstringin->inp,DBF_STRING,pstringin->val);
	pstringin->udf = FALSE;
        break;
    case (PV_LINK) :
    case (DB_LINK) :
    case (CA_LINK) :
        break;
    default :
	recGblRecordError(S_db_badField,(void *)pstringin,
		"devSiSoft (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_stringin(pstringin)
    struct stringinRecord	*pstringin;
{
    long status;

    status = dbGetLink(&pstringin->inp,DBR_STRING,pstringin->val,0,0);
    if(RTN_SUCCESS(status)) pstringin->udf=FALSE;
    return(status);
}
