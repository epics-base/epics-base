/* devLoSoft.c */
/* share/src/dev  $Id$ */

/* devLoSoft.c - Device Support Routines for Soft Longout Output */
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
 *              Advanced Photon Lource
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
#include	<longoutRecord.h>

/* added for Channel Access Links */
long dbCaAddOutlink();
long dbCaPutLink();
long init_record();

/* Create the dset for devLoSoft */
long write_longout();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_longout;
}devLoSoft={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	write_longout};
 

static long init_record(plongout)
struct longoutRecord *plongout;
{
 
long status;
 
    if (plongout->out.type == PV_LINK)
        status = dbCaAddOutlink(&(plongout->out), (void *) plongout, "VAL");
    else
        status = 0L;
 
    return status;
 
} /* end init_record() */

static long write_longout(plongout)
    struct longoutRecord	*plongout;
{
    long status,options,nrequest;

    /* longout.out must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (plongout->out.type) {
    case (CONSTANT) :
	break;
    case (DB_LINK) :
	status = dbPutLink(&plongout->out.value.db_link,(struct dbCommon *)plongout,DBR_LONG,
	               &plongout->val,1L);
        if(status!=0) {
                recGblSetSevr(plongout,LINK_ALARM,VALID_ALARM);
        }
	break;
    case (CA_LINK) :
        options = 0L;
        nrequest = 1L;
        status = dbCaPutLink(&(plongout->out), &options, &nrequest);
	break;
    default :
        if(recGblSetSevr(plongout,SOFT_ALARM,VALID_ALARM)){
		if(plongout->stat!=SOFT_ALARM) {
			recGblRecordError(S_db_badField,plongout,
			    "devLoSoft (write_longout) Illegal OUT field");
		}
	}
    }
    return(0);
}
