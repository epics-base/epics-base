/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devMbbiDirectSoftRaw.c */
/* base/src/dev $Id$ */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Matthew Needes
 *      Date:            10-08-93
 */
#include	<stdlib.h>
#include	<stdio.h>
#include	<string.h>

#include	"alarm.h"
#include	"dbDefs.h"
#include	"dbAccess.h"
#include	"recGbl.h"
#include        "recSup.h"
#include	"devSup.h"
#include	"mbbiDirectRecord.h"
#define epicsExportSharedSymbols
#include "shareLib.h"

/* Create the dset for devMbbiDirectSoftRaw */
static long init_record();
static long read_mbbi();
epicsShareDef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_mbbi;
}devMbbiDirectSoftRaw={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_mbbi
};

static long init_record(mbbiDirectRecord *pmbbi)
{

    /* mbbi.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pmbbi->inp.type) {
    case (CONSTANT) :
	recGblInitConstantLink(&pmbbi->inp,DBF_ULONG,&pmbbi->rval);
        break;
    case (DB_LINK) :
    case (PV_LINK) :
    case (CA_LINK) :
        break;
    default :
	recGblRecordError(S_db_badField,(void *)pmbbi,
		"devMbbiDirectSoftRaw (init_record) Illegal INP field");
	return(S_db_badField);
    }
    /*to preserve old functionality*/
    if(pmbbi->nobt == 0) pmbbi->mask = 0xffffffff;
    pmbbi->mask <<= pmbbi->shft;
    return(0);
}

static long read_mbbi(mbbiDirectRecord *pmbbi)
{
    long status;

    status = dbGetLink(&pmbbi->inp,DBR_LONG,&pmbbi->rval,0,0);
    if(status==0) pmbbi->rval &= pmbbi->mask;
    return(0);
}
