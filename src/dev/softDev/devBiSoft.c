/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devBiSoft.c */
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
#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "biRecord.h"
#include "epicsExport.h"

/* Create the dset for devBiSoft */
static long init_record();
static long read_bi();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_bi;
}devBiSoft={
	5,
	NULL,
	NULL,
	init_record,
	NULL,
	read_bi
};
epicsExportAddress(dset,devBiSoft);

static long init_record(biRecord *pbi)
{

    /* bi.inp must be a CONSTANT or a PV_LINK or a DB_LINK or a CA_LINK */
    switch (pbi->inp.type) {
    case (CONSTANT) :
        if(recGblInitConstantLink(&pbi->inp,DBF_ENUM,&pbi->val))
            pbi->udf = FALSE;
        break;
    case (DB_LINK) :
    case (PV_LINK) :
    case (CA_LINK) :
        break;
    default :
	recGblRecordError(S_db_badField,(void *)pbi,
		"devBiSoft (init_record) Illegal INP field");
	return(S_db_badField);
    }
    return(0);
}

static long read_bi(biRecord *pbi)
{
    long status;

    status = dbGetLink(&pbi->inp, DBR_USHORT, &pbi->val,0,0);
    if(pbi->inp.type!=CONSTANT && RTN_SUCCESS(status)) pbi->udf=FALSE;
    return(2);
}
