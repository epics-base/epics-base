/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* devMbboDirectSoftRaw.c */
/*
 * Original Author: Janet Anderson
 * Date: 10-08-93
 */

#include <stdio.h>

#include "dbAccess.h"
#include "epicsTypes.h"
#include "devSup.h"
#include "mbboDirectRecord.h"
#include "epicsExport.h"

static long init_record(mbboDirectRecord *prec)
{
    if (prec->nobt == 0)
        prec->mask = 0xffffffff;

    prec->mask <<= prec->shft;

    return 2; /* Don't convert */
}

static long write_mbbo(mbboDirectRecord	*prec)
{
    epicsUInt32 data;

    data = prec->rval & prec->mask;
    dbPutLink(&prec->out, DBR_ULONG, &data, 1);
    return 0;
}

/* Create the dset for devMbboDirectSoftRaw */
struct {
    dset common;
    DEVSUPFUN write;
} devMbboDirectSoftRaw = {
    {5, NULL, NULL, init_record, NULL},
    write_mbbo
};
epicsExportAddress(dset, devMbboDirectSoftRaw);
