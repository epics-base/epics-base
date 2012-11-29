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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "epicsTime.h"
#include "recGbl.h"
#include "devSup.h"
#include "link.h"
#include "lsiRecord.h"
#include "epicsExport.h"

static long init_record(lsiRecord *prec)
{
    /* Handle CONSTANT links */
    if (prec->inp.type == CONSTANT) {
        char tmp[MAX_STRING_SIZE];

        if (recGblInitConstantLink(&prec->inp, DBF_STRING, tmp)) {
            long len = prec->sizv;

            strncpy(prec->val, tmp, len);
            prec->len = strlen(prec->val) + 1;
            prec->udf = FALSE;
        }
    }
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
