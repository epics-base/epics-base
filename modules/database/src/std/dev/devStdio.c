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

#include "lsoRecord.h"
#include "printfRecord.h"
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


/* lso device support */

static long add_lso(dbCommon *pcommon) {
    lsoRecord *prec = (lsoRecord *) pcommon;
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

static long del_lso(dbCommon *pcommon) {
    lsoRecord *prec = (lsoRecord *) pcommon;

    prec->dpvt = NULL;
    return 0;
}

static struct dsxt dsxtLsoStdio = {
    add_lso, del_lso
};

static long init_lso(int pass)
{
    if (pass == 0) devExtend(&dsxtLsoStdio);
    return 0;
}

static long write_lso(lsoRecord *prec)
{
    struct outStream *pstream = (struct outStream *)prec->dpvt;
    if (pstream)
        pstream->print("%s\n", prec->val);
    return 0;
}

lsodset devLsoStdio = {
    5, NULL, init_lso, NULL, NULL, write_lso
};
epicsExportAddress(dset, devLsoStdio);


/* printf device support */

static long add_printf(dbCommon *pcommon) {
    printfRecord *prec = (printfRecord *) pcommon;
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

static long del_printf(dbCommon *pcommon) {
    printfRecord *prec = (printfRecord *) pcommon;

    prec->dpvt = NULL;
    return 0;
}

static struct dsxt dsxtPrintfStdio = {
    add_printf, del_printf
};

static long init_printf(int pass)
{
    if (pass == 0) devExtend(&dsxtPrintfStdio);
    return 0;
}

static long write_printf(printfRecord *prec)
{
    struct outStream *pstream = (struct outStream *)prec->dpvt;
    if (pstream)
        pstream->print("%s\n", prec->val);
    return 0;
}

printfdset devPrintfStdio = {
    5, NULL, init_printf, NULL, NULL, write_printf
};
epicsExportAddress(dset, devPrintfStdio);


/* stringout device support */

static long add_stringout(dbCommon *pcommon) {
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

static long del_stringout(dbCommon *pcommon) {
    stringoutRecord *prec = (stringoutRecord *) pcommon;

    prec->dpvt = NULL;
    return 0;
}

static struct dsxt dsxtSoStdio = {
    add_stringout, del_stringout
};

static long init_stringout(int pass)
{
    if (pass == 0) devExtend(&dsxtSoStdio);
    return 0;
}

static long write_stringout(stringoutRecord *prec)
{
    struct outStream *pstream = (struct outStream *)prec->dpvt;
    if (pstream)
        pstream->print("%s\n", prec->val);
    return 0;
}

static struct {
    dset common;
    DEVSUPFUN write;
} devSoStdio = {
    {5, NULL, init_stringout, NULL, NULL}, write_stringout
};
epicsExportAddress(dset, devSoStdio);
