/* devAoSoftRaw.c */
/* share/src/dev @(#)devAoSoftRaw.c	1.6     4/6/92 */

/* Device Support Routines for soft raw Analog Output Records*/
/*
 *      Author:         Janet Anderson
 *      Date:           09-25-91
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
 * .02  03-04-92        jba     Added special_linconv
 * .03	03-13-92	jba	ANSI C changes
 * .04  10-10-92        jba     replaced code with recGblGetLinkValue call
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
#include	<link.h>
#include	<special.h>
#include	<aoRecord.h>

/* added for Channel Access Links */
long dbCaAddOutlink();
long dbCaPutLink();
static long init_record();

/* Create the dset for devAoSoftRaw */
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
}devAoSoftRaw={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	write_ao,
	special_linconv};

static long init_record(pao)
struct aoRecord *pao;
{

long status;

    if (pao->out.type == PV_LINK)
	status = dbCaAddOutlink(&(pao->out), (void *) pao, "RVAL");
    else
	status = 0L;

    return status;

} /* end init_record() */

static long write_ao(pao)
    struct aoRecord	*pao;
{
    long status,nRequest=1;

    status = recGblPutLinkValue(&(pao->out),(void *)pao,DBR_LONG,&(pao->rval),&nRequest);

    return(status);
}

static long special_linconv(pao,after)
    struct aoRecord	*pao;
    int after;
{

    if(!after) return(0);

    return(0);
}

