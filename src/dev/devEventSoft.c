/* devEventSoft.c */
/* share/src/dev @(#)devEvent.c	1.1     12/17/91 */

/* devEventSoft.c - Device Support Routines for Soft Event Input */
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
 * .00  12-13-91        jba     Initial definition
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
#include	<eventRecord.h>


/* Create the dset for devEventSoft */
long init_record();
long read_event();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_event;
}devEventSoft={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_event};

static long init_record(pevent)
    struct eventRecord	*pevent;
{
    char message[100];
    int precision;

    /* event.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pevent->inp.type) {
    case (CONSTANT) :
	if(pevent->inp.value.value!=0.0){
	        pevent->enum=pevent->inp.value.value;
		}
	pevent->udf= FALSE;
        break;
    case (PV_LINK) :
        break;
    case (DB_LINK) :
        break;
    case (CA_LINK) :
        break;
    default :
	strcpy(message,pevent->name);
	strcat(message,": devEventSoft (init_record) Illegal INP field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long read_event(pevent)
    struct eventRecord	*pevent;
{
    char message[100];
    long status=0,options,nRequest;

    /* event.inp must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (pevent->inp.type) {
    case (CONSTANT) :
        break;
    case (DB_LINK) :
        options=0;
        nRequest=1;
        status = dbGetLink(&(pevent->inp.value.db_link),pevent,DBR_SHORT,
                pevent->enum,&options,&nRequest);
        if(status!=0) {
                recGblSetSevr(pevent,LINK_ALARM,VALID_ALARM);
        } else pevent->udf = FALSE;
        break;
    case (CA_LINK) :
        break;
    default :
        if(recGblSetSevr(pevent,SOFT_ALARM,VALID_ALARM)){
                if(pevent->stat!=SOFT_ALARM) {
                        strcpy(message,pevent->name);
                        strcat(message,": devEventSoft (read_event) Illegal INP field");
                        errMessage(S_db_badField,message);
                }
        }
    }
    return(status);
}
