/* devSiSoft.c */
 /* share/src/dev   $Id$ */

/* devSiSoft.c - Device Support Routines for Soft String Input */
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
 * .01  mm-dd-yy        iii     Comment
*/


#include	<vxWorks.h>
#include	<types.h>
#include	<stdioLib.h>

#include	<alarm.h>
#include	<dbAccess.h>
#include	<dbDefs.h>
#include	<devSup.h>
#include	<link.h>
#include	<module_types.h>
#include	<stringinRecord.h>


/* Create the dset for devSiSoft */
long init_record();
long read_stringin();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_stringin;
}devSiSoft={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_stringin};

static long init_record(pstringin)
    struct stringinRecord	*pstringin;
{
    char message[100];
    int precision;

    /* stringin.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pstringin->inp.type) {
    case (CONSTANT) :
	if(pstringin->inp.value.value!=0.0){
	        sprintf(pstringin->val,"%-14.7g",pstringin->inp.value.value);
		pstringin->udf= FALSE;
		}
        break;
    case (PV_LINK) :
        break;
    case (DB_LINK) :
        break;
    case (CA_LINK) :
        break;
    default :
	strcpy(message,pstringin->name);
	strcat(message,": devSiSoft (init_record) Illegal INP field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long read_stringin(pstringin)
    struct stringinRecord	*pstringin;
{
    char message[100];
    long status=0,options,nRequest;

    /* stringin.inp must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (pstringin->inp.type) {
    case (CONSTANT) :
        break;
    case (DB_LINK) :
        options=0;
        nRequest=1;
        status = dbGetLink(&(pstringin->inp.value.db_link),pstringin,DBR_STRING,
                pstringin->val,&options,&nRequest);
        if(status!=0) {
                if(pstringin->nsev<VALID_ALARM) {
                        pstringin->nsev = VALID_ALARM;
                        pstringin->nsta = LINK_ALARM;
                }
        } else pstringin->udf = FALSE;
        break;
    case (CA_LINK) :
        break;
    default :
        if(pstringin->nsev<VALID_ALARM) {
                pstringin->nsev = VALID_ALARM;
                pstringin->nsta = SOFT_ALARM;
                if(pstringin->stat!=SOFT_ALARM) {
                        strcpy(message,pstringin->name);
                        strcat(message,": devSiSoft (read_stringin) Illegal INP field");
                        errMessage(S_db_badField,message);
                }
        }
    }
    return(status);
}
