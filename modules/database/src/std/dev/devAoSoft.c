/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* devAoSoft.c */

/* Device Support Routines for soft Analog Output Records*/
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
#include "link.h"
#include "special.h"
#include "aoRecord.h"
#include "epicsExport.h"

/* Create the dset for devAoSoft */
static long init_record(dbCommon *pcommon);
static long write_ao(aoRecord *prec);

aodset devAoSoft = {
    {6, NULL, NULL, init_record, NULL},
    write_ao, NULL
};
epicsExportAddress(dset, devAoSoft);

static long init_record(dbCommon *pcommon)
{

    long status=0;
    status = 2;
    return status;

} /* end init_record() */

static long write_ao(aoRecord *prec)
{
    long status;

    status = dbPutLink(&prec->out,DBR_DOUBLE, &prec->oval,1);

    return(status);
}
