/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* devLoSoftCallback.c */
/*
 *      Author:  Marty Kraimer
 *      Date:    04NOV2003
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
#include "longoutRecord.h"
#include "epicsExport.h"

/* Create the dset for devLoSoftCallback */
static long write_longout(longoutRecord *prec);
struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	write_longout;
}devLoSoftCallback={
	5,
	NULL,
	NULL,
	NULL,
	NULL,
	write_longout
};
epicsExportAddress(dset,devLoSoftCallback);

static long write_longout(longoutRecord	*prec)
{
    struct link *plink = &prec->out;
    long status;

    if (prec->pact)
        return 0;

    status = dbPutLinkAsync(plink, DBR_LONG, &prec->val, 1);
    if (!status)
        prec->pact = TRUE;
    else if (status == S_db_noLSET)
        status = dbPutLink(plink, DBR_LONG, &prec->val, 1);

    return status;
}

