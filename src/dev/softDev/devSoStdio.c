/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* $Id$ */

#include <stdio.h>
#include <string.h>

#include "dbCommon.h"
#include "devSup.h"
#include "recGbl.h"
#include "recSup.h"
#include "epicsExport.h"

#include "stringoutRecord.h"

static long add(dbCommon *pcommon) {
    stringoutRecord *prec = (stringoutRecord *) pcommon;

    if (prec->out.type != INST_IO)
        return S_dev_badOutType;

    if (strcmp(prec->out.value.instio.string, "stdout") == 0) {
        prec->dpvt = stdout;
    } else if (strcmp(prec->out.value.instio.string, "stderr") == 0) {
        prec->dpvt = stderr;
    } else
        return -1;
    return 0;
}

static long del(dbCommon *pcommon) {
    stringoutRecord *prec = (stringoutRecord *) pcommon;

    prec->dpvt = NULL;
    return 0;
}

static struct dsxt dsxtSoStdio = {
    add, del
};

static long init(int pass)
{
    if (pass == 0) devExtend(&dsxtSoStdio);
    return 0;
}

static long write(stringoutRecord *prec)
{
    if (prec->dpvt)
        fprintf((FILE *)prec->dpvt, "%s\n", prec->val);
    return 0;
}

/* Create the dset for devSoStdio */
static struct {
    dset common;
    DEVSUPFUN write;
} devSoStdio = {
    {5, NULL, init, NULL, NULL},  write
};
epicsExportAddress(dset, devSoStdio);
