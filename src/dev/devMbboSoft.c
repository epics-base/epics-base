/* devMbboSoft.c */
/* share/src/dev $Id$ */

/* devMbboSoft.c - Device Support Routines for  Soft Multibit Binary Output*/
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
#include	<module_types.h>
#include	<mbboRecord.h>

/* added for Channel Access Links */
long dbCaAddOutlink();
long dbCaPutLink();
long init_record();

/* Create the dset for devMbboSoft */
long write_mbbo();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_mbbo;
}devMbboSoft={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	write_mbbo};



static long init_record(pmbbo)
struct mbboRecord *pmbbo;
{
 
    long status=0;
 
    if (pmbbo->out.type == PV_LINK)
        status = dbCaAddOutlink(&(pmbbo->out), (void *) pmbbo, "VAL");
 
    /* dont convert */
    if ( status == 0 ) status=2;

    return status;
 
} /* end init_record() */

static long write_mbbo(pmbbo)
    struct mbboRecord	*pmbbo;
{
    long status,nRequest=1;

    status = recGblPutLinkValue(&(pmbbo->out),(void *)pmbbo,DBR_USHORT,&(pmbbo->val),
              &nRequest);

    if(RTN_SUCCESS(status)) pmbbo->udf=FALSE;

    return(0);
}
