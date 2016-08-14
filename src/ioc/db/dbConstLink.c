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

#include <stdio.h>

#include "dbDefs.h"

#define epicsExportSharedSymbols
#include "dbAccessDefs.h"
#include "dbAddr.h"
#include "dbCommon.h"
#include "dbConstLink.h"
#include "dbConvertFast.h"
#include "dbConvertJSON.h"
#include "dbFldTypes.h"
#include "dbLink.h"
#include "link.h"

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

/**************************** Member functions ****************************/

static long dbConstLoadScalar(struct link *plink, short dbrType, void *pbuffer)
{
    if (!plink->value.constantStr)
        return S_db_badField;

    /* Constant scalars are always numeric */
    if (dbrType == DBF_MENU || dbrType == DBF_ENUM || dbrType == DBF_DEVICE)
        dbrType = DBF_USHORT;

    return dbFastPutConvertRoutine[DBR_STRING][dbrType]
            (plink->value.constantStr, pbuffer, NULL);
}

static long dbConstLoadArray(struct link *plink, short dbrType, void *pbuffer,
        long *pnReq)
{
    if (!plink->value.constantStr)
        return S_db_badField;

    /* No support for arrays of choice types */
    if (dbrType == DBF_MENU || dbrType == DBF_ENUM || dbrType == DBF_DEVICE)
        return S_db_badField;

    return dbPutConvertJSON(plink->value.constantStr, dbrType, pbuffer, pnReq);
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
    NULL,
    dbConstLoadScalar,
    dbConstLoadArray,
    NULL,
    NULL, dbConstGetNelements,
    dbConstGetValue,
    NULL, NULL, NULL,
    NULL, NULL,
    NULL, NULL,
    NULL, NULL,
    NULL
};

