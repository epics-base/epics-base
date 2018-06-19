/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* devMbboDirectSoft.c */
/*
 * Original Author: Bob Dalesio
 * Date: 10-08-93
 */

#include <stdio.h>

#include "dbAccess.h"
#include "devSup.h"
#include "mbboDirectRecord.h"
#include "epicsExport.h"

static long write_mbbo(mbboDirectRecord	*prec)
{
    dbPutLink(&prec->out, DBR_USHORT, &prec->val, 1);
    return 0;
}

/* Create the dset for devMbboDirectSoft */
struct {
    dset common;
    DEVSUPFUN write;
} devMbboDirectSoft = {
    {5, NULL, NULL, NULL, NULL},
    write_mbbo
};
epicsExportAddress(dset, devMbboDirectSoft);
