/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devLiSoft.c */
/* base/src/dev $Id$ */
/*
 *      Author:		Janet Anderson
 *      Date:   	09-23-91
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
#include	"longinRecord.h"
#define epicsExportSharedSymbols
#include "shareLib.h"


/* Create the dset for devLiSoft */
static long init_record();
static long read_longin();

epicsShareExtern struct {
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
	read_longin
};

static long init_record(longinRecord *plongin)
{

    /* longin.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (plongin->inp.type) {
    case (CONSTANT) :
        if(recGblInitConstantLink(&plongin->inp,DBF_LONG,&plongin->val))
            plongin->udf = FALSE;
	break;
    case (PV_LINK) :
    case (DB_LINK) :
    case (CA_LINK) :
        break;
    default :
	recGblRecordError(S_db_badField,(void *)plongin,
	    "devLiSoft (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_longin(longinRecord *plongin)
{
    long status;

    status = dbGetLink(&plongin->inp,DBR_LONG, &plongin->val,0,0);
    if(plongin->inp.type!=CONSTANT && RTN_SUCCESS(status)) plongin->udf=FALSE;
    return(status);
}
