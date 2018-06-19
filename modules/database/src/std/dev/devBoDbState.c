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
#include "dbAccessDefs.h"
#include "boRecord.h"
#include "epicsExport.h"

#define DEVSUPNAME "devBoDbState"

static long add_record (struct dbCommon *pdbc)
{
    boRecord *prec = (boRecord *) pdbc;

    if (INST_IO != prec->out.type) {
        recGblRecordError(S_db_badField, (void *) prec, DEVSUPNAME ": Illegal OUT field");
        return(S_db_badField);
    }

    if (!(prec->dpvt = dbStateFind(prec->out.value.instio.string)) &&
        prec->out.value.instio.string &&
        '\0' != *prec->out.value.instio.string) {
        errlogSevPrintf(errlogInfo, DEVSUPNAME ": Creating new db state '%s'\n",
                        prec->out.value.instio.string);
        prec->dpvt = dbStateCreate(prec->out.value.instio.string);
    }
    return 0;
}

static long del_record (struct dbCommon *pdbc)
{
    boRecord *prec = (boRecord *) pdbc;
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

static long write_bo(boRecord *prec)
{
    if (prec->val)
        dbStateSet(prec->dpvt);
    else
        dbStateClear(prec->dpvt);
    return 0;
}

static struct {
        long		number;
        DEVSUPFUN	report;
        DEVSUPFUN	init;
        DEVSUPFUN	init_record;
        DEVSUPFUN	get_ioint_info;
        DEVSUPFUN	write_bo;
} devBoDbState = {
        5,
        NULL,
        init,
        NULL,
        NULL,
        write_bo
};

epicsExportAddress(dset, devBoDbState);
