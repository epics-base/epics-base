/* SPDX-FileCopyrightText: 1998 Argonne National Laboratory */

/* SPDX-License-Identifier: EPICS */

/* devXxxSoft.c */
/* Example device support module */

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alarm.h"
#include "cvtTable.h"
#include "dbDefs.h"
#include "dbAccess.h"
#include "recGbl.h"
#include "recSup.h"
#include "devSup.h"
#include "link.h"
#include "xxxRecord.h"
#include "epicsExport.h"

static long init_record(dbCommon *prec)
{
    struct xxxRecord *pxxx = (struct xxxRecord *) prec;

    if (recGblInitConstantLink(&pxxx->inp, DBF_DOUBLE, &pxxx->val))
         pxxx->udf = FALSE;

    return 0;
}

static long read_xxx(struct xxxRecord *pxxx)
{
    long status = dbGetLink(&(pxxx->inp), DBF_DOUBLE, &(pxxx->val), 0, 0);

     /* If get was successful VAL is now defined */
    if (!status)
        pxxx->udf = FALSE;

    return 0;
}

/*Create the dset for devXxxSoft */
xxxdset devXxxSoft = {
    { 5, NULL, NULL, init_record, NULL },
    read_xxx,
};
epicsExportAddress(dset, devXxxSoft);
