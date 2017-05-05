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
#include <string.h>

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
    const char *pstr = plink->value.constantStr;
    size_t len;

    if (!pstr)
        return S_db_badField;
    len = strlen(pstr);

    /* Choice values must be numeric */
    if (dbrType == DBF_MENU || dbrType == DBF_ENUM || dbrType == DBF_DEVICE)
        dbrType = DBF_USHORT;

    if (*pstr == '[' && pstr[len-1] == ']') {
        /* Convert from JSON array */
        long nReq = 1;

        return dbPutConvertJSON(pstr, dbrType, pbuffer, &nReq);
    }

    return dbFastPutConvertRoutine[DBR_STRING][dbrType]
            (pstr, pbuffer, NULL);
}

static long dbConstLoadLS(struct link *plink, char *pbuffer, epicsUInt32 size,
    epicsUInt32 *plen)
{
    const char *pstr = plink->value.constantStr;

    if (!pstr)
        return S_db_badField;

    return dbLSConvertJSON(pstr, pbuffer, size, plen);
}

static long dbConstLoadArray(struct link *plink, short dbrType, void *pbuffer,
        long *pnReq)
{
    const char *pstr = plink->value.constantStr;

    if (!pstr)
        return S_db_badField;

    /* Choice values must be numeric */
    if (dbrType == DBF_MENU || dbrType == DBF_ENUM || dbrType == DBF_DEVICE)
        dbrType = DBF_USHORT;

    return dbPutConvertJSON(pstr, dbrType, pbuffer, pnReq);
}

static long dbConstGetNelements(const struct link *plink, long *nelements)
{
    *nelements = 0;
    return 0;
}

static long dbConstGetValue(struct link *plink, short dbrType, void *pbuffer,
        long *pnRequest)
{
    if (pnRequest)
        *pnRequest = 0;
    return 0;
}

static lset dbConst_lset = {
    1, 0, /* Constant, not Volatile */
    NULL, NULL,
    dbConstLoadScalar,
    dbConstLoadLS,
    dbConstLoadArray,
    NULL,
    NULL, dbConstGetNelements,
    dbConstGetValue,
    NULL, NULL, NULL,
    NULL, NULL,
    NULL, NULL,
    NULL, NULL,
    NULL, NULL
};
