/*************************************************************************\
* Copyright (c) 2020 Michael Davidsaver
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef USE_TYPED_DSET
#  define USE_TYPED_DSET
#endif

#include <dbAccess.h>
#include <recGbl.h>

// include by stdstringinRecord.h
#include "epicsTypes.h"
#include "link.h"
#include "epicsMutex.h"
#include "ellLib.h"
#include "devSup.h"
#include "epicsTime.h"

#define epicsExportSharedSymbols

#include "stdstringinRecord.h"
#include <epicsExport.h>

namespace {

long init_record(struct dbCommon *pcommon)
{
    stdstringinRecord *prec = (stdstringinRecord *)pcommon;
    VString ival;
    ival.vtype = &vfStdString;

    if (recGblInitConstantLink(&prec->inp, DBR_VFIELD, &ival)) {
        prec->udf = FALSE;
        prec->val = ival.value;
    }

    return 0;
}

long readLocked(struct link *pinp, void *dummy)
{
    stdstringinRecord *prec = (stdstringinRecord *) pinp->precord;

    VString ival;
    ival.vtype = &vfStdString;

    long status;
    if(!(status = dbGetLink(pinp, DBR_VFIELD, &ival, 0, 0))) {
        prec->val = ival.value;

    } else if (status==S_db_badDbrtype) {
        char str[MAX_STRING_SIZE];
        if(!(status = dbGetLink(pinp, DBR_STRING, str, 0, 0))) {
            str[MAX_STRING_SIZE-1] = '\0';
            prec->val = str;
        }
    }
    if (status) return status;

    if (dbLinkIsConstant(&prec->tsel) &&
        prec->tse == epicsTimeEventDeviceTime)
        dbGetTimeStamp(pinp, &prec->time);

    return status;
}

long read_ssi(stdstringinRecord* prec)
{
    long status = dbLinkDoLocked(&prec->inp, readLocked, NULL);

    if (status == S_db_noLSET)
        status = readLocked(&prec->inp, NULL);

    if (!status && !dbLinkIsConstant(&prec->inp))
        prec->udf = FALSE;

    return status;
}

stdstringindset devSSiSoft = {
    {5, NULL, NULL, &init_record, NULL},
    &read_ssi
};

}
extern "C" {
epicsExportAddress(dset, devSSiSoft);
}
