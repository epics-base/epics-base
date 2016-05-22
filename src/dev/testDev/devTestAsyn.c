/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* Device Support for testing asynchronous processing
 *
 * This is a universal device support, it should work with
 * almost any record type, but it doesn't actually do any
 * I/O operations so it's not particularly useful other than
 * for testing asynchronous record processing.
 */

#include <stdio.h>
#include <stdlib.h>

#include "callback.h"
#include "devSup.h"
#include "dbCommon.h"

#include "epicsExport.h"

static long addRec(struct dbCommon *prec)
{
    prec->dpvt = calloc(1, sizeof(CALLBACK));
    return !prec->dpvt;
}

static long delRec(struct dbCommon *prec)
{
    if (prec->dpvt) {
        callbackCancelDelayed((CALLBACK *)prec->dpvt);
        free(prec->dpvt);
    }
    return 0;
}

static struct dsxt dsxtTestAsyn = {
    addRec, delRec
};

static long init(int pass)
{
    if (!pass) devExtend(&dsxtTestAsyn);
    return 0;
}

static long process(struct dbCommon *prec)
{
    if (prec->pact) {
        printf("Completed asynchronous processing: %s\n", prec->name);
        return 0;
    }
    if (prec->disv <= 0) return 2;

    printf("Starting asynchronous processing: %s\n", prec->name);
    prec->pact = TRUE;
    if (prec->dpvt) {
        callbackRequestProcessCallbackDelayed((CALLBACK *)prec->dpvt,
            prec->prio, prec, (double)prec->disv);
    }
    return 0;
}

/* Create the dset for devTestAsyn */
struct {
    dset common;
    DEVSUPFUN   read;
    DEVSUPFUN   misc;
} devTestAsyn = {
    {6, NULL, init, NULL, NULL}, process, NULL
};
epicsExportAddress(dset, devTestAsyn);
