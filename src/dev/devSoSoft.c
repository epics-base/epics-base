/* devSoSoft.c */
 /* share/src/dev   $Id$ */

/* devSoSoft.c - Device Support Routines for Soft String Output */
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
#include	<stringoutRecord.h>

/* added for Channel Access Links */
long dbCaAddOutlink();
long dbCaPutLink();
long init_record();

/* Create the dset for devSoSoft */
long write_stringout();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_stringout;
}devSoSoft={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	write_stringout};
 

static long init_record(pstringout)
struct stringoutRecord *pstringout;
{
 
long status;
 
    if (pstringout->out.type == PV_LINK)
        status = dbCaAddOutlink(&(pstringout->out), (void *) pstringout, "VAL");
    else
        status = 0L;
 
    return status;
 
} /* end init_record() */

static long write_stringout(pstringout)
    struct stringoutRecord	*pstringout;
{
    long status,nRequest=1;

    status = recGblPutLinkValue(&(pstringout->out),(void *)pstringout,DBR_STRING,pstringout->val,
              &nRequest);

    return(status);
}
