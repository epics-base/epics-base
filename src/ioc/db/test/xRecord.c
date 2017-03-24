/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2010 Brookhaven National Laboratory.
* Copyright (c) 2010 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

/*
 *  Author: Andrew Johnson <anj@aps.anl.gov>
 *          Ralph Lange <Ralph.Lange@bessy.de>
 */

#include "dbAccessDefs.h"
#include "recSup.h"
#include "recGbl.h"
#include "devSup.h"
#include "dbScan.h"

#define GEN_SIZE_OFFSET
#include "xRecord.h"

#include <epicsExport.h>

#include "devx.h"

static long init_record(struct dbCommon *pcommon, int pass)
{
    struct xRecord *prec = (struct xRecord *)pcommon;
    long ret = 0;
    xdset *xset = (xdset*)prec->dset;
    if(!pass) return 0;

    if(!xset) {
        recGblRecordError(S_dev_noDSET, prec, "x: init_record");
        return S_dev_noDSET;
    }
    if(xset->init_record)
        ret = (*xset->init_record)(prec);
    return ret;
}

static long process(struct dbCommon *pcommon)
{
    struct xRecord *prec = (struct xRecord *)pcommon;
    long ret = 0;
    xdset *xset = (xdset*)prec->dset;

    if(prec->clbk)
        (*prec->clbk)(prec);
    prec->pact = TRUE;
    if(xset  && xset->process)
        ret = (*xset->process)(prec);
    recGblGetTimeStamp(prec);
    recGblFwdLink(prec);
    prec->pact = FALSE;
    return ret;
}

static rset xRSET = {
    RSETNUMBER, NULL, NULL, init_record, process
};
epicsExportAddress(rset,xRSET);
