/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* dbConstLink.c
 *
 *      Original Authors: Bob Dalesio, Marty Kraimer
 *      Current Author: Andrew Johnson
 */

#include <stddef.h>
#include <stdio.h>

#include "cvtFast.h"
#include "dbDefs.h"
#include "errlog.h"

#define epicsExportSharedSymbols
#include "dbAccessDefs.h"
#include "dbAddr.h"
#include "dbBase.h"
#include "dbCommon.h"
#include "dbConvertFast.h"
#include "dbConvert.h"
#include "dbFldTypes.h"
#include "dbLink.h"
#include "link.h"
#include "recGbl.h"

/***************************** Constant Links *****************************/

/* Forward definition */
static lset dbConst_lset;

void dbConstInitLink(struct link *plink)
{
    plink->lset = &dbConst_lset;
}

void dbConstAddLink(struct link *plink)
{
    plink->lset = &dbConst_lset;
}

static long dbConstLoadLink(struct link *plink, short dbrType, void *pbuffer)
{
    if (!plink->value.constantStr)
        return S_db_badField;

    plink->lset = &dbConst_lset;

    /* Constant strings are always numeric */
    if (dbrType == DBF_MENU || dbrType == DBF_ENUM || dbrType == DBF_DEVICE)
        dbrType = DBF_USHORT;

    return dbFastPutConvertRoutine[DBR_STRING][dbrType]
            (plink->value.constantStr, pbuffer, NULL);
}

static long dbConstGetNelements(const struct link *plink, long *nelements)
{
    *nelements = 0;
    return 0;
}

static long dbConstGetValue(struct link *plink, short dbrType, void *pbuffer,
        epicsEnum16 *pstat, epicsEnum16 *psevr, long *pnRequest)
{
    if (pnRequest)
        *pnRequest = 0;
    return 0;
}

static lset dbConst_lset = {
    dbConstLoadLink,
    NULL,
    NULL,
    NULL, dbConstGetNelements,
    dbConstGetValue,
    NULL, NULL, NULL,
    NULL, NULL,
    NULL, NULL,
    NULL,
    NULL
};

