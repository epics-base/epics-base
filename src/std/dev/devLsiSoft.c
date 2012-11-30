/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Long String Input soft device support
 *
 * Author: Andrew Johnson
 * Date: 2012-11-28
 */

#include "dbAccess.h"
#include "epicsTime.h"
#include "link.h"
#include "lsiRecord.h"
#include "epicsExport.h"

static long init_record(lsiRecord *prec)
{
    dbLoadLinkLS(&prec->inp, prec->val, prec->sizv, &prec->len);

    return 0;
}

static long read_string(lsiRecord *prec)
{
    long status = dbGetLinkLS(&prec->inp, prec->val, prec->sizv, &prec->len);

    if (!status &&
        prec->tsel.type == CONSTANT &&
        prec->tse == epicsTimeEventDeviceTime)
        dbGetTimeStamp(&prec->inp, &prec->time);

    return status;
}

lsidset devLsiSoft = {
    5, NULL, NULL, init_record, NULL, read_string
};
epicsExportAddress(dset, devLsiSoft);
