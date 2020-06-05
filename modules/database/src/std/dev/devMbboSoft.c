/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* devMbboSoft.c */
/*
 *      Original Author: Bob Dalesio
 *      Current Author:  Marty Kraimer
 *      Date:            6-1-90
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
#include "mbboRecord.h"
#include "epicsExport.h"

/* Create the dset for devMbboSoft */
static long init_record(dbCommon *pcommon);
static long write_mbbo(mbboRecord *prec);

mbbodset devMbboSoft = {
    {5, NULL, NULL, init_record, NULL},
    write_mbbo
};
epicsExportAddress(dset, devMbboSoft);

static long init_record(dbCommon *pcommon)
{
    /*dont convert*/
    return 2;

} /* end init_record() */

static long write_mbbo(mbboRecord *prec)
{
    dbPutLink(&prec->out,DBR_USHORT, &prec->val,1);
    return 0;
}
