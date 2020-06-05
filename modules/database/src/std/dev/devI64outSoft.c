/*************************************************************************\
* Copyright (c) 2016 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 *      Original Author: Janet Anderson
 *      Date:           09-23-91
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
#include "int64outRecord.h"
#include "epicsExport.h"

static long init_record(dbCommon *common)
{
    return 0;
}

static long write_int64out(int64outRecord *prec)
{
    dbPutLink(&prec->out, DBR_INT64, &prec->val,1);
    return 0;
}

/* Create the dset for devI64outSoft */
int64outdset devI64outSoft = {
    { 5, NULL, NULL, init_record, NULL }, write_int64out
};
epicsExportAddress(dset, devI64outSoft);

