/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 *      Author: Janet Anderson
 *      Date:   21APR1991
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "stringoutRecord.h"
#include "epicsExport.h"

/* Create the dset for devSoSoft */
static long write_stringout(stringoutRecord *prec);

stringoutdset devSoSoft = {
    {5, NULL, NULL, NULL, NULL},
    write_stringout
};
epicsExportAddress(dset, devSoSoft);

static long write_stringout(stringoutRecord *prec)
{
    long status;

    status = dbPutLink(&prec->out, DBR_STRING, prec->val, 1);
    return status;
}
