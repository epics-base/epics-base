/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* devBoSoft.c - Device Support Routines for  Soft Binary Output*/
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
#include "boRecord.h"
#include "epicsExport.h"

/* Create the dset for devBoSoft */
static long init_record(dbCommon *pcommon);
static long write_bo(boRecord *prec);

bodset devBoSoft = {
    {5, NULL, NULL, init_record, NULL},
    write_bo
};
epicsExportAddress(dset, devBoSoft);

static long init_record(dbCommon *pcommon)
{
   long status=0;

    /* don't convert */
   status=2;
   return status;

} /* end init_record() */

static long write_bo(boRecord *prec)
{
    long status;

    status = dbPutLink(&prec->out,DBR_USHORT,&prec->val,1);

    return(status);
}
