/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devWfSoft.c */
/* base/src/dev $Id$ */

/* devWfSoft.c - Device Support Routines for soft Waveform Records*/
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
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
#include	"link.h"
#include	"waveformRecord.h"
#define epicsExportSharedSymbols
#include "shareLib.h"

/* Create the dset for devWfSoft */
static long init_record();
static long read_wf();
epicsShareDef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_wf;
}devWfSoft={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_wf};


static long init_record(waveformRecord *pwf)
{

    /* wf.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pwf->inp.type) {
    case (CONSTANT) :
	pwf->nord = 0;
	break;
    case (PV_LINK) :
    case (DB_LINK) :
    case (CA_LINK) :
	break;
    default :
	recGblRecordError(S_db_badField,(void *)pwf,
		"devWfSoft (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_wf(waveformRecord *pwf)
{
    long status,nRequest;

    nRequest=pwf->nelm;
    status = dbGetLink(&pwf->inp,pwf->ftvl,pwf->bptr, 0,&nRequest);
    /*If dbGetLink got no values leave things as they were*/
    if(nRequest>0) pwf->nord = nRequest;

    return(0);
}
