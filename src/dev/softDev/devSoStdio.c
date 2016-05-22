/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <stdio.h>
#include <string.h>

#include "dbCommon.h"
#include "devSup.h"
#include "errlog.h"
#include "recGbl.h"
#include "recSup.h"

#include "stringoutRecord.h"
#include "epicsExport.h"

typedef int (*PRINTFFUNC)(const char *fmt, ...);

static int stderrPrintf(const char *fmt, ...);
static int logPrintf(const char *fmt, ...);


static struct outStream {
    const char *name;
    PRINTFFUNC print;
} outStreams[] = {
    {"stdout", printf},
    {"stderr", stderrPrintf},
    {"errlog", logPrintf},
    {NULL, NULL}
};

static int stderrPrintf(const char *fmt, ...) {
    va_list pvar;
    int retval;

    va_start(pvar, fmt);
    retval = vfprintf(stderr, fmt, pvar);
    va_end (pvar);

    return retval;
}

static int logPrintf(const char *fmt, ...) {
    va_list pvar;
    int retval;

    va_start(pvar, fmt);
    retval = errlogVprintf(fmt, pvar);
    va_end (pvar);

    return retval;
}

static long add(dbCommon *pcommon) {
    stringoutRecord *prec = (stringoutRecord *) pcommon;
    struct outStream *pstream;

    if (prec->out.type != INST_IO)
        return S_dev_badOutType;

    for (pstream = outStreams; pstream->name; ++pstream) {
        if (strcmp(prec->out.value.instio.string, pstream->name) == 0) {
            prec->dpvt = pstream;
            return 0;
        }
    }
    prec->dpvt = NULL;
    return -1;
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

static long write_string(stringoutRecord *prec)
{
    struct outStream *pstream = (struct outStream *)prec->dpvt;
    if (pstream)
        pstream->print("%s\n", prec->val);
    return 0;
}

/* Create the dset for devSoStdio */
static struct {
    dset common;
    DEVSUPFUN write;
} devSoStdio = {
    {5, NULL, init, NULL, NULL}, write_string
};
epicsExportAddress(dset, devSoStdio);
