/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devSiSoft.c */
/* base/src/dev $Id$ */
/*
 *      Author:		Janet Anderson
 *      Date:   	04-21-91
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
#include	"stringinRecord.h"
#define epicsExportSharedSymbols
#include "shareLib.h"

/* Create the dset for devSiSoft */
static long init_record();
static long read_stringin();
static struct {
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
	read_stringin
};
epicsShareExtern dset *pdevSiSoft;
epicsShareDef dset *pdevSiSoft = (dset *)&devSiSoft;

static long init_record(stringinRecord *pstringin)
{

    /* stringin.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK*/
    switch (pstringin->inp.type) {
    case (CONSTANT) :
       if(recGblInitConstantLink(&pstringin->inp,DBF_STRING,pstringin->val))
           pstringin->udf = FALSE;
       break;
    case (PV_LINK) :
    case (DB_LINK) :
    case (CA_LINK) :
        break;
    default :
	recGblRecordError(S_db_badField,(void *)pstringin,
		"devSiSoft (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_stringin(stringinRecord *pstringin)
{
    long status;

    status = dbGetLink(&pstringin->inp,DBR_STRING,pstringin->val,0,0);
    if(pstringin->inp.type!=CONSTANT && RTN_SUCCESS(status)) pstringin->udf=FALSE;
    return(status);
}
