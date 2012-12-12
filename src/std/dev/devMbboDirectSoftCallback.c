/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* devMbboDirectSoftCallback.c */
/*
 * Original Author: Marty Kraimer
 * Date: 04NOV2003
 */

#include <stdio.h>

#include "alarm.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "devSup.h"
#include "mbboDirectRecord.h"
#include "epicsExport.h"

static long write_mbbo(mbboDirectRecord *prec)
{
    struct link *plink = &prec->out;
    long status;

    if (prec->pact)
        return 0;

    if (plink->type != CA_LINK) {
        status = dbPutLink(plink, DBR_USHORT, &prec->val, 1);
        return status;
    }

    status = dbCaPutLinkCallback(plink, DBR_USHORT, &prec->val, 1,
        dbCaCallbackProcess, plink);
    if (status) {
        recGblSetSevr(prec, LINK_ALARM, INVALID_ALARM);
        return status;
    }

    prec->pact = TRUE;
    return 0;
}

/* Create the dset for devMbboSoft */
struct {
    dset common;
    DEVSUPFUN write;
} devMbboDirectSoftCallback = {
    {5, NULL, NULL, NULL, NULL},
    write_mbbo
};
epicsExportAddress(dset, devMbboDirectSoftCallback);
