/* devLiSoft.c */
/* base/src/dev $Id$ */

/* devLiSoft.c - Device Support Routines for Soft Longin Input */
/*
 *      Author:		Janet Anderson
 *      Date:   	09-23-91
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
#include	<longinRecord.h>
/* Added for Channel Access Links */
long dbCaAddInlink();
long dbCaGetLink();

/* Create the dset for devLiSoft */
static long init_record();
static long read_longin();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_longin;
}devLiSoft={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_longin};

static long init_record(plongin)
    struct longinRecord	*plongin;
{
    long status;

    /* longin.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (plongin->inp.type) {
    case (CONSTANT) :
	if(recGblInitConstantLink(&plongin->inp,DBF_LONG,&plongin->val))
	    plongin->udf = FALSE;
	break;
    case (PV_LINK) :
    case (DB_LINK) :
        status = recGblInitFastInLink(&(plongin->inp), (void *) plongin, DBR_LONG, "VAL");

        if (status)
           return(status);

        break;
    default :
	recGblRecordError(S_db_badField,(void *)plongin,
	    "devLiSoft (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_longin(plongin)
    struct longinRecord	*plongin;
{
    long status;

    status = recGblGetFastLink(&(plongin->inp), (void *)plongin, &(plongin->val));

    if(RTN_SUCCESS(status)) plongin->udf=FALSE;

    return(status);
}
