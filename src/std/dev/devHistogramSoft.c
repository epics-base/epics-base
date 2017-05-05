/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devHistogramSoft.c */
/*
 *      Author:		Janet Anderson
 *      Date:		07/02/91
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "cvtTable.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "histogramRecord.h"
#include "epicsExport.h"

/* Create the dset for devHistogramSoft */
static long init_record(histogramRecord *prec);
static long read_histogram(histogramRecord *prec);
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	read_histogram;
	DEVSUPFUN	special_linconv;
}devHistogramSoft={
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	read_histogram,
	NULL
};
epicsExportAddress(dset,devHistogramSoft);

static long init_record(histogramRecord	*prec)
{
    if (recGblInitConstantLink(&prec->svl,DBF_DOUBLE,&prec->sgnl))
        prec->udf = FALSE;

    return 0;
}

static long read_histogram(histogramRecord *prec)
{
    dbGetLink(&prec->svl, DBR_DOUBLE, &prec->sgnl, 0, 0);
    return 0; /*add count*/
}
