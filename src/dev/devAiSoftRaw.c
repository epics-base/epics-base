/* devAiSoftRaw.c */
/* share/src/dev $Id$ */

/* devAiSoftRaw.c - Device Support Routines for soft Analog Input Records*/
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
 *
 *	Experimental Physics and Industrial Control System (EPICS)
 *
 *	Copyright 1991, the Regents of the University of California,
 *	and the University of Chicago Board of Governors.
 *
 *	This software was produced under  U.S. Government contracts:
 *	(W-7405-ENG-36) at the Los Alamos National Laboratory,
 *	and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *	Initial development by:
 *		The Controls and Automation Group (AT-8)
 *		Ground Test Accelerator
 *		Accelerator Technology Division
 *		Los Alamos National Laboratory
 *
 *	Co-developed with
 *		The Controls and Computing Group
 *		Accelerator Systems Division
 *		Advanced Photon Source
 *		Argonne National Laboratory
 *
 * Modification Log:
 * -----------------
 * .01  11-11-91        jba     Moved set of alarm stat and sevr to macros
 * .02  03-04-92        jba     Added special_linconv 
 * .03	03-13-92	jba	ANSI C changes
 * 	...
 */


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>
#include	<string.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<aiRecord.h>
/* Added for Channel Access Links */
long dbCaAddInlink();
long dbCaGetLink();

/* Create the dset for devAiSoftRaw */
long init_record();
long read_ai();
long special_linconv();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_ai;
	DEVSUPFUN	special_linconv;
}devAiSoftRaw={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	read_ai,
	special_linconv};


static long init_record(pai)
    struct aiRecord	*pai;
{
    long status;

    /* ai.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pai->inp.type) {
    case (CONSTANT) :
	pai->rval = pai->inp.value.value;
	break;
    case (PV_LINK) :
        status = dbCaAddInlink(&(pai->inp), (void *) pai, "RVAL");
        if(status) return(status);
	break;
    case (DB_LINK) :
	break;
    case (CA_LINK) :
	break;
    default :
	recGblRecordError(S_db_badField,pai,
		"devAiSoftRaw (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_ai(pai)
    struct aiRecord	*pai;
{
    long status,options,nRequest;

    /* ai.inp must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (pai->inp.type) {
    case (CONSTANT) :
	break;
    case (DB_LINK) :
	options=0;
	nRequest=1;
	status = dbGetLink(&(pai->inp.value.db_link),(struct dbCommon *)pai,DBR_LONG,
		&(pai->rval),&options,&nRequest);
        if(status!=0) {
                recGblSetSevr(pai,LINK_ALARM,VALID_ALARM);
        }

	break;
    case (CA_LINK) :
        if (dbCaGetLink(&(pai->inp)))
        {
            recGblSetSevr(pai,LINK_ALARM,VALID_ALARM);
        } 
        else 
            pai->udf = FALSE;
	break;
    default :
        if(recGblSetSevr(pai,SOFT_ALARM,VALID_ALARM)){
		if(pai->stat!=SOFT_ALARM) {
			recGblRecordError(S_db_badField,pai,
			   "devAiSoftRaw (read_ai) Illegal INP field");
		}
	}
    }
    return(0);
}

static long special_linconv(pai,after)
     struct aiRecord   *pai;
     int after;
{

	if(!after) return(0);

	return(0);
}
