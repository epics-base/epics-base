/* devPtSoft.c */
/* share/src/dev  $Id$ */

/* devPtSoft.c - Device Support Routines for Soft Pulse Generator Output */
/*
 *      Author:		Janet Anderson
 *      Date:		6-1-91
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
 *      ...
 */


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>

#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<module_types.h>
#include	<pulseTrainRecord.h>


/* Create the dset for devPtSoft */
long write_pt();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_pt;
}devPtSoft={
	5,
	NULL,
	NULL,
	NULL,
	NULL,
	write_pt};


static long write_pt(ppt)
    struct pulseTrainRecord	*ppt;
{
    char message[100];
    long status;

    /* pt.out must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (ppt->out.type) {
    case (CONSTANT) :
        break;
    case (DB_LINK) :
        status = dbPutLink(&ppt->out.value.db_link,ppt,DBR_SHORT,&ppt->val,1L);
        if(status!=0) {
                recGblSetSevr(ppt,LINK_ALARM,VALID_ALARM);
        } else ppt->udf=FALSE;
        break;
    case (CA_LINK) :
        break;
    default :
        if(recGblSetSevr(ppt,WRITE_ALARM,VALID_ALARM)){
                if(ppt->stat!=SOFT_ALARM) {
                        strcpy(message,ppt->name);
                        strcat(message,": devPtSoft (write_pt) Illegal OUT field");
                        errMessage(S_db_badField,message);
                }
        }
    }
    return(0);
}
