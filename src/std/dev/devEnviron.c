/*************************************************************************\
* Copyright (c) 2016 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* devEnviron.c */

#include <stdlib.h>
#include <string.h>

#include "alarm.h"
#include "dbCommon.h"
#include "devSup.h"
#include "errlog.h"
#include "recGbl.h"
#include "recSup.h"

#include "lsiRecord.h"
#include "stringinRecord.h"
#include "epicsExport.h"

/* lsi device support */

static long add_lsi(dbCommon *pcommon) {
    lsiRecord *prec = (lsiRecord *) pcommon;

    if (prec->inp.type != INST_IO)
        return S_dev_badInpType;

    return 0;
}

static long del_lsi(dbCommon *pcommon) {
    return 0;
}

static struct dsxt dsxtLsiEnviron = {
    add_lsi, del_lsi
};

static long init_lsi(int pass)
{
    if (pass == 0)
        devExtend(&dsxtLsiEnviron);

    return 0;
}

static long read_lsi(lsiRecord *prec)
{
    const char *val = getenv(prec->inp.value.instio.string);

    if (val) {
        strncpy(prec->val, val, prec->sizv);
        prec->val[prec->sizv - 1] = 0;
        prec->len = strlen(prec->val);
        prec->udf = FALSE;
    }
    else {
        prec->val[0] = 0;
        prec->len = 1;
        prec->udf = TRUE;
        recGblSetSevr(prec, UDF_ALARM, prec->udfs);
    }

    return 0;
}

lsidset devLsiEnviron = {
    5, NULL, init_lsi, NULL, NULL, read_lsi
};
epicsExportAddress(dset, devLsiEnviron);


/* stringin device support */

static long add_stringin(dbCommon *pcommon) {
    stringinRecord *prec = (stringinRecord *) pcommon;

    if (prec->inp.type != INST_IO)
        return S_dev_badInpType;

    return 0;
}

static long del_stringin(dbCommon *pcommon) {
    return 0;
}

static struct dsxt dsxtSiEnviron = {
    add_stringin, del_stringin
};

static long init_stringin(int pass)
{
    if (pass == 0)
        devExtend(&dsxtSiEnviron);

    return 0;
}

static long read_stringin(stringinRecord *prec)
{
    const char *val = getenv(prec->inp.value.instio.string);

    if (val) {
        strncpy(prec->val, val, MAX_STRING_SIZE);
        prec->val[MAX_STRING_SIZE - 1] = 0;
        prec->udf = FALSE;
    }
    else {
        prec->val[0] = 0;
        prec->udf = TRUE;
        recGblSetSevr(prec, UDF_ALARM, prec->udfs);
    }

    return 0;
}

static struct {
    dset common;
    DEVSUPFUN read;
} devSiEnviron = {
    {5, NULL, init_stringin, NULL, NULL}, read_stringin
};
epicsExportAddress(dset, devSiEnviron);
