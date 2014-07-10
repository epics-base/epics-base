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

#define GEN_SIZE_OFFSET
#include "xRecord.h"

#include <epicsExport.h>

static long process(xRecord *prec)
{
    prec->pact = TRUE;
    recGblGetTimeStamp(prec);
    recGblFwdLink(prec);
    prec->pact = FALSE;
    return 0;
}

static rset xRSET = {
    RSETNUMBER, NULL, NULL, NULL, process
};
epicsExportAddress(rset,xRSET);
