/* devAoSoft.c */
/* share/src/dev @(#)devAoSoft.c	1.13     4/6/92 */

/* Device Support Routines for soft Analog Output Records*/
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
 * .03	03-13-92	jba	ANSI C changes
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
long init_record();

/* Create the dset for devAoSoft */
long write_ao();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_ao;
	DEVSUPFUN	special_linconv;
}devAoSoft={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	write_ao,
	NULL};


static long init_record(pao)
struct aoRecord *pao;
{

long status;

    if (pao->out.type == PV_LINK)
	status = dbCaAddOutlink(&(pao->out), (void *) pao, "OVAL");
    else
	status = 0L;

    return status;

} /* end init_record() */

static long write_ao(pao)
    struct aoRecord	*pao;
{
    char message[100];
    long status;
/* added for Channel Access Links */
long options;
long nrequest;

    /* ao.out must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (pao->out.type) {
    case (CONSTANT) :
	break;
    case (DB_LINK) :
	status = dbPutLink(&pao->out.value.db_link,(struct dbCommon *)pao,DBR_DOUBLE,
		&pao->oval,1L);
        if(status!=0) {
                recGblSetSevr(pao,LINK_ALARM,VALID_ALARM);
        }
	break;
    case (CA_LINK) :
        options = 0L;
        nrequest = 1L;
        status = dbCaPutLink(&(pao->out), &options, &nrequest);
	break;
    default :
        if(recGblSetSevr(pao,SOFT_ALARM,VALID_ALARM)){
		if(pao->stat!=SOFT_ALARM) {
			strcpy(message,pao->name);
			strcat(message,": devAoSoft (write_ao) Illegal OUT field");
			errMessage(S_db_badField,message);
		}
	}
    }
    return(0);
}
