/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*
 *      Author: Andrew Johnson
 *      Date:   28 Sept 2012
 */

#include "dbAccess.h"
#include "printfRecord.h"
#include "epicsExport.h"

static long write_string(printfRecord *prec)
{
    return dbPutLinkLS(&prec->out, prec->val, prec->len);
}

printfdset devPrintfSoft = {
    5, NULL, NULL, NULL, NULL, write_string
};
epicsExportAddress(dset, devPrintfSoft);

