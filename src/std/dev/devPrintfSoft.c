/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Revision-Id$ */
/*
 *      Author: Andrew Johnson
 *      Date:   28 Sept 2012
 */

#include "dbAccess.h"
#include "printfRecord.h"
#include "epicsExport.h"

static long write_string(printfRecord *prec)
{
    struct link *plink = &prec->out;
    int dtyp = dbGetLinkDBFtype(plink);

    if (dtyp < 0)
        return 0;   /* Not connected */

    if (dtyp == DBR_CHAR || dtyp == DBF_UCHAR)
        return dbPutLink(plink, dtyp, prec->val, prec->len);

    return dbPutLink(plink, DBR_STRING, prec->val, 1);
}

printfdset devPrintfSoft = {
    5, NULL, NULL, NULL, NULL, write_string
};
epicsExportAddress(dset, devPrintfSoft);

