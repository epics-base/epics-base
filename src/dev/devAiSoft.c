/* devAiSoft.c */
/* share/src/dev $Id$ */

/* devAiSoft.c - Device Support Routines for soft Analog Input Records*/
/*
 *      Original Author: Bob Dalesio
 *      Current Author: Marty Kraimer
 *      Date:  3/6/91
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
 * 	...
 */



#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>

#include	<alarm.h>
#include	<cvtTable.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include        <recSup.h>
#include	<devSup.h>
#include	<link.h>
#include	<aiRecord.h>

/* Create the dset for devAiSoft */
long init_record();
long read_ai();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_ai;
	DEVSUPFUN	special_linconv;
}devAiSoft={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	read_ai,
	NULL};


static long init_record(pai)
    struct aiRecord	*pai;
{
    char message[100];

    /* ai.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pai->inp.type) {
    case (CONSTANT) :
	pai->val = pai->inp.value.value;
	pai->udf = FALSE;
	break;
    case (PV_LINK) :
	break;
    case (DB_LINK) :
	break;
    case (CA_LINK) :
	break;
    default :
	strcpy(message,pai->name);
	strcat(message,": devAiSoft (init_record) Illegal INP field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    /* Make sure record processing routine does not perform any conversion*/
    pai->linr=0;
    return(0);
}

static long read_ai(pai)
    struct aiRecord	*pai;
{
    char message[100];
    long status,options,nRequest;

    /* ai.inp must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (pai->inp.type) {
    case (CONSTANT) :
	break;
    case (DB_LINK) :
	options=0;
	nRequest=1;
	status = dbGetLink(&(pai->inp.value.db_link),pai,DBR_DOUBLE,
		&(pai->val),&options,&nRequest);
	if(status!=0) {
                recGblSetSevr(pai,LINK_ALARM,VALID_ALARM);
	} else pai->udf = FALSE;
	break;
    case (CA_LINK) :
	break;
    default :
        if(recGblSetSevr(pai,SOFT_ALARM,VALID_ALARM)){
		if(pai->stat!=SOFT_ALARM) {
			strcpy(message,pai->name);
			strcat(message,": devAiSoft (read_ai) Illegal INP field");
			errMessage(S_db_badField,message);
		}
	}
    }
    return(2); /*don't convert*/
}
