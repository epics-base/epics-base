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
    char message[100];
    long status;
/* added for Channel Access Links */
long options;
long nrequest;

    /* stringout.out must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (pstringout->out.type) {
    case (CONSTANT) :
	break;
    case (DB_LINK) :
	status = dbPutLink(&pstringout->out.value.db_link,(struct dbCommon *)pstringout,DBR_STRING,
		pstringout->val,1L);
        if(status!=0) {
                recGblSetSevr(pstringout,LINK_ALARM,VALID_ALARM);
        }
	break;
    case (CA_LINK) :
        options = 0L;
        nrequest = 1L;
        status = dbCaPutLink(&(pstringout->out), &options, &nrequest);
	break;
    default :
        if(recGblSetSevr(pstringout,SOFT_ALARM,VALID_ALARM)){
		if(pstringout->stat!=SOFT_ALARM) {
			strcpy(message,pstringout->name);
			strcat(message,": devSoSoft (write_stringout) Illegal OUT field");
			errMessage(S_db_badField,message);
		}
	}
    }
    return(0);
}
