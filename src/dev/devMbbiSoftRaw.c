/* devMbbiSoftRaw.c */
/* share/src/dev $Id$ */

/* devMbbiSoftRaw.c - Device Support Routines for  Soft Multibit Binary Input*/
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
 * .01  mm-dd-yy        iii     Comment
 * .02  mm-dd-yy        iii     Comment
 *      ...
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
#include	<mbbiRecord.h>


/* Create the dset for devMbbiSoftRaw */
long init_record();
long read_mbbi();

struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_mbbi;
}devMbbiSoftRaw={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_mbbi};


static long init_record(pmbbi)
    struct mbbiRecord	*pmbbi;
{
    char message[100];
    /* mbbi.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pmbbi->inp.type) {
    case (CONSTANT) :
        pmbbi->rval = pmbbi->inp.value.value;
        break;
    case (PV_LINK) :
        break;
    case (DB_LINK) :
        break;
    case (CA_LINK) :
        break;
    default :
	strcpy(message,pmbbi->name);
	strcat(message,": devMbbiSoftRaw (init_record) Illegal INP field");
	errMessage(S_db_badField,message);
	return(S_db_badField);
    }
    return(0);
}

static long read_mbbi(pmbbi)
    struct mbbiRecord	*pmbbi;
{
    char message[100];
    long status,options,nRequest;

    /* mbbi.inp must be a CONSTANT or a DB_LINK or a CA_LINK*/
    switch (pmbbi->inp.type) {
    case (CONSTANT) :
        break;
    case (DB_LINK) :
        options=0;
        nRequest=1;
        status = dbGetLink(&(pmbbi->inp.value.db_link),pmbbi,DBR_ULONG,
                &(pmbbi->rval),&options,&nRequest);
        if(status!=0) {
                if(pmbbi->nsev<VALID_ALARM) {
                        pmbbi->nsev = VALID_ALARM;
                        pmbbi->nsta = LINK_ALARM;
                }
        }
        break;
    case (CA_LINK) :
        break;
    default :
        if(pmbbi->nsev<VALID_ALARM) {
                pmbbi->nsev = VALID_ALARM;
                pmbbi->nsta = SOFT_ALARM;
                if(pmbbi->stat!=SOFT_ALARM) {
                        strcpy(message,pmbbi->name);
                        strcat(message,": devMbbiSoftRaw (read_mbbi) Illegal INP field");
                        errMessage(S_db_badField,message);
                }
        }
    }
    return(0);
}
