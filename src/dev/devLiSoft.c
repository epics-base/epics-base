/* devLiSoft.c */
/* share/src/dev  $Id$ */

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
*/


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<module_types.h>
#include	<longinRecord.h>


/* Create the dset for devLiSoft */
long init_record();
long read_longin();

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
    char message[100];

    /* longin.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (plongin->inp.type) {
    case (CONSTANT) :
	plongin->val = plongin->inp.value.value;
	plongin->udf = FALSE;
	break;
    case (PV_LINK) :
        break;
    case (DB_LINK) :
        break;
    case (CA_LINK) :
        break;
    default :
	strcpy(message,plongin->name);
	strcat(message,": devLiSoft (init_record) Illegal INP field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long read_longin(plongin)
    struct longinRecord	*plongin;
{
    char message[100];
    long status=0,options,nRequest;

    /* longin.inp must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (plongin->inp.type) {
    case (CONSTANT) :
        break;
    case (DB_LINK) :
        options=0;
        nRequest=1;
        status = dbGetLink(&(plongin->inp.value.db_link),(struct dbCommon *)plongin,DBR_LONG,
               &plongin->val,&options,&nRequest);
        if(status!=0) {
                recGblSetSevr(plongin,LINK_ALARM,VALID_ALARM);
        } else plongin->udf = FALSE;
        break;
    case (CA_LINK) :
        break;
    default :
        if(recGblSetSevr(plongin,SOFT_ALARM,VALID_ALARM)){
                if(plongin->stat!=SOFT_ALARM) {
                        strcpy(message,plongin->name);
                        strcat(message,": devLiSoft (read_longin) Illegal INP field");
                        errMessage(S_db_badField,message);
                }
        }
    }
    return(status);
}
