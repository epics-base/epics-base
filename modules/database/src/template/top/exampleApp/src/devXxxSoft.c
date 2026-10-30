/* SPDX-FileCopyrightText: 2026 UChicago Argonne LLC */
/* SPDX-License-Identifier: EPICS */

/* devXxxSoft.c */

/* Sample device & driver support interfaces */

#include <dbAccess.h>
#include <dbDefs.h>
#include <devSup.h>
#include <drvSup.h>
#include <epicsStdio.h>
#include <link.h>
#include <recGbl.h>
#include "xxxRecord.h"
#include <epicsExport.h>

/* Device Support Routines */

static long init_record(dbCommon *pcommon)
{
    struct xxxRecord *prec = (struct xxxRecord *) pcommon;

    if (recGblInitConstantLink(&prec->inp, DBF_DOUBLE, &prec->val))
         prec->udf = FALSE;

    return 0;
}

static long read_xxx(struct xxxRecord *prec)
{
    long status = dbGetLink(&prec->inp, DBF_DOUBLE, &prec->val, 0, 0);

    /* If get was successful VAL is now defined */
    if (!status)
        prec->udf = FALSE;

    return 0;
}

/* devXxxSoft Device Entry Table */
xxxdset devXxxSoft = {
    { 5, NULL /* report */, NULL /* init */,
        init_record, NULL /* get_ioint_info */ },
    read_xxx,
};
epicsExportAddress(dset, devXxxSoft);


/* Driver Routines */

/* The report method is called by the iocsh command 'dbior'.
 * Use level to show more or different information about
 * the state of the driver software, connections etc.
 */
static long xxxReport(int level) {
    printf("xxxDriver: Report called, level %d\n", level);
    return 0;
}

/* xxxDriver Entry Table */
drvet xxxDriver = {
    2,
    xxxReport,
    NULL /* init, rarely used */
};
epicsExportAddress(drvet, xxxDriver);
