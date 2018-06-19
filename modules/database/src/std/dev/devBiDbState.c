/*************************************************************************\
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *  Author: Ralph Lange <Ralph.Lange@bessy.de>
 */

#include "errlog.h"
#include "dbState.h"
#include "devSup.h"
#include "recGbl.h"
#include "dbLink.h"
#include "dbAccessDefs.h"
#include "biRecord.h"
#include "epicsExport.h"

#define DEVSUPNAME "devBiDbState"

static long add_record (struct dbCommon *pdbc)
{
    biRecord *prec = (biRecord *) pdbc;

    if (INST_IO != prec->inp.type) {
        recGblRecordError(S_db_badField, (void *) prec, DEVSUPNAME ": Illegal INP field");
        return(S_db_badField);
    }

    if (!(prec->dpvt = dbStateFind(prec->inp.value.instio.string)) &&
        prec->inp.value.instio.string &&
        '\0' != *prec->inp.value.instio.string) {
        errlogSevPrintf(errlogInfo, DEVSUPNAME ": Creating new db state '%s'\n",
                        prec->inp.value.instio.string);
        prec->dpvt = dbStateCreate(prec->inp.value.instio.string);
    }
    return 0;
}

static long del_record (struct dbCommon *pdbc)
{
    biRecord *prec = (biRecord *) pdbc;
    prec->dpvt = NULL;
    return 0;
}

static struct dsxt myDsxt = {
    add_record,
    del_record
};

static long init(int pass)
{
    if (pass == 0)
        devExtend(&myDsxt);
    return 0;
}

static long read_bi(biRecord *prec)
{
    if (prec->dpvt) {
        prec->val = dbStateGet(prec->dpvt);
        prec->udf = FALSE;
    }

    return 2;
}

static struct {
        long		number;
        DEVSUPFUN	report;
        DEVSUPFUN	init;
        DEVSUPFUN	init_record;
        DEVSUPFUN	get_ioint_info;
        DEVSUPFUN	read_bi;
} devBiDbState = {
        5,
        NULL,
        init,
        NULL,
        NULL,
        read_bi
};

epicsExportAddress(dset, devBiDbState);
