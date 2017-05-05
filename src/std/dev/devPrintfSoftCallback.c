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

#include "alarm.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "printfRecord.h"
#include "epicsExport.h"

static long write_string(printfRecord *prec)
{
    struct link *plink = &prec->out;
    int dtyp = dbGetLinkDBFtype(plink);
    long len = prec->len;
    long status;

    if (prec->pact || dtyp < 0)
        return 0;

    if (dtyp != DBR_CHAR && dtyp != DBF_UCHAR) {
        dtyp = DBR_STRING;
        len = 1;
    }

    status = dbPutLinkAsync(plink, dtyp, prec->val, len);
    if (!status)
        prec->pact = TRUE;
    else if (status == S_db_noLSET)
        status = dbPutLink(plink, dtyp, prec->val, len);

    return status;
}

printfdset devPrintfSoftCallback = {
    5, NULL, NULL, NULL, NULL, write_string
};
epicsExportAddress(dset, devPrintfSoftCallback);
