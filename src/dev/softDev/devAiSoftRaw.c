/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devAiSoftRaw.c */
/* base/src/dev $Id$ */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "cvtTable.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "dbEvent.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "aiRecord.h"
#define epicsExportSharedSymbols
#include "shareLib.h"

/* Create the dset for devAiSoftRaw */
static long init_record(aiRecord *pai);
static long read_ai(aiRecord *pai);
static struct {
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
	NULL
};
epicsShareDef dset *pdevAiSoftRaw = (dset *)&devAiSoftRaw;

static long init_record(aiRecord *pai)
{
    /* ai.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pai->inp.type) {
    case (CONSTANT) :
	recGblInitConstantLink(&pai->inp,DBF_LONG,&pai->rval);
	break;
    case (PV_LINK) :
    case (DB_LINK) :
    case (CA_LINK) :
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pai,
		"devAiSoftRaw (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_ai(aiRecord *pai)
{
    long status;

    status = dbGetLink(&(pai->inp),DBR_LONG,&(pai->rval),0,0);
    return(0);
}
