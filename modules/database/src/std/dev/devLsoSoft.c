/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Long String Output soft device support
 *
 * Author: Andrew Johnson
 * Date: 2012-11-29
 */

#include "dbAccess.h"
#include "lsoRecord.h"
#include "epicsExport.h"

static long write_string(lsoRecord *prec)
{
    return dbPutLinkLS(&prec->out, prec->val, prec->len);
}

lsodset devLsoSoft = {
    5, NULL, NULL, NULL, NULL, write_string
};
epicsExportAddress(dset, devLsoSoft);
