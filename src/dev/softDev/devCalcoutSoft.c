/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devCalcoutSoft.c */

/*
 *      Author:  Marty Kraimer
 *      Date:    05DEC2003
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
#include "link.h"
#include "special.h"
#include "postfix.h"
#include "calcoutRecord.h"
#include "epicsExport.h"

static long write_calcout();
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write;
	DEVSUPFUN	special_linconv;
}devCalcoutSoft={
	6,
	NULL,
	NULL,
	NULL,
	NULL,
	write_calcout,
	NULL};
epicsExportAddress(dset,devCalcoutSoft);

static long write_calcout(calcoutRecord *pcalcout)
{
    return dbPutLink(&pcalcout->out,DBR_DOUBLE, &pcalcout->oval,1);
}
