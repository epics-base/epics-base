/*************************************************************************\
* Copyright (c) 2017 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* lnkState.c */

/*  Usage:
 *      {state:green}
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "alarm.h"
#include "dbDefs.h"
#include "errlog.h"
#include "epicsAssert.h"
#include "epicsString.h"
#include "epicsTypes.h"
#include "dbAccessDefs.h"
#include "dbCommon.h"
#include "dbConvertFast.h"
#include "dbLink.h"
#include "dbJLink.h"
#include "dbStaticLib.h"
#include "dbStaticPvt.h"
#include "dbState.h"
#include "recGbl.h"
#include "epicsExport.h"


typedef long (*FASTCONVERT)();

typedef struct state_link {
    jlink jlink;        /* embedded object */
    char *name;
    short val;
    short invert;
    dbStateId state;
} state_link;

static lset lnkState_lset;


/*************************** jlif Routines **************************/

static jlink* lnkState_alloc(short dbfType)
{
    state_link *slink;

    if (dbfType == DBF_FWDLINK) {
        errlogPrintf("lnkState: DBF_FWDLINK not supported\n");
        return NULL;
    }

    slink = calloc(1, sizeof(struct state_link));
    if (!slink) {
        errlogPrintf("lnkState: calloc() failed.\n");
        return NULL;
    }

    slink->name = NULL;
    slink->state = NULL;
    slink->invert = 0;
    slink->val = 0;

    return &slink->jlink;
}

static void lnkState_free(jlink *pjlink)
{
    state_link *slink = CONTAINER(pjlink, struct state_link, jlink);

    free(slink->name);
    free(slink);
}

static jlif_result lnkState_string(jlink *pjlink, const char *val, size_t len)
{
    state_link *slink = CONTAINER(pjlink, struct state_link, jlink);

    if (len > 1 && val[0] == '!') {
        slink->invert = 1;
        val++; len--;
    }

    slink->name = epicsStrnDup(val, len);
    return jlif_continue;
}

static struct lset* lnkState_get_lset(const jlink *pjlink)
{
    return &lnkState_lset;
}

static void lnkState_report(const jlink *pjlink, int level, int indent)
{
    state_link *slink = CONTAINER(pjlink, struct state_link, jlink);

    printf("%*s'state': \"%s\" = %s%s\n", indent, "",
        slink->name, slink->invert ? "! " : "", slink->val ? "TRUE" : "FALSE");
}

/*************************** lset Routines **************************/

static void lnkState_open(struct link *plink)
{
    state_link *slink = CONTAINER(plink->value.json.jlink,
        struct state_link, jlink);

    slink->state = dbStateCreate(slink->name);
}

static void lnkState_remove(struct dbLocker *locker, struct link *plink)
{
    state_link *slink = CONTAINER(plink->value.json.jlink,
        struct state_link, jlink);

    free(slink->name);
    free(slink);

    plink->value.json.jlink = NULL;
}

static int lnkState_getDBFtype(const struct link *plink)
{
    return DBF_SHORT;
}

static long lnkState_getElements(const struct link *plink, long *nelements)
{
    *nelements = 1;
    return 0;
}

static long lnkState_getValue(struct link *plink, short dbrType, void *pbuffer,
    long *pnRequest)
{
    state_link *slink = CONTAINER(plink->value.json.jlink,
        struct state_link, jlink);
    FASTCONVERT conv;

    if(INVALID_DB_REQ(dbrType))
        return S_db_badDbrtype;

    conv = dbFastPutConvertRoutine[DBR_SHORT][dbrType];

    slink->val = slink->invert ^ dbStateGet(slink->state);
    return conv(&slink->val, pbuffer, NULL);
}

static long lnkState_putValue(struct link *plink, short dbrType,
    const void *pbuffer, long nRequest)
{
    state_link *slink = CONTAINER(plink->value.json.jlink,
        struct state_link, jlink);
    short val;
    const char *pstr;

    if (nRequest == 0)
        return 0;

    switch(dbrType) {
    case DBR_CHAR:
    case DBR_UCHAR:
        val = !! *(const epicsInt8 *) pbuffer;
        break;

    case DBR_SHORT:
    case DBR_USHORT:
        val = !! *(const epicsInt16 *) pbuffer;
        break;

    case DBR_LONG:
    case DBR_ULONG:
        val = !! *(const epicsInt32 *) pbuffer;
        break;

    case DBR_INT64:
    case DBR_UINT64:
        val = !! *(const epicsInt64 *) pbuffer;
        break;

    case DBR_FLOAT:
        val = !! *(const epicsFloat32 *) pbuffer;
        break;

    case DBR_DOUBLE:
        val = !! *(const epicsFloat64 *) pbuffer;
        break;

    case DBR_STRING:    /* Only "" and "0" are FALSE */
        pstr = (const char *) pbuffer;
        val = (pstr[0] != 0) && ((pstr[0] != '0') || (pstr[1] != 0));
        break;

    default:
        return S_db_badDbrtype;
    }
    slink->val = val;

    val ^= slink->invert;
    (val ? dbStateSet : dbStateClear)(slink->state);

    return 0;
}

/************************* Interface Tables *************************/

static lset lnkState_lset = {
    0, 0, /* not constant, always connected */
    lnkState_open, lnkState_remove,
    NULL, NULL, NULL,
    NULL, lnkState_getDBFtype, lnkState_getElements,
    lnkState_getValue,
    NULL, NULL, NULL,
    NULL, NULL,
    NULL, NULL,
    lnkState_putValue, NULL,
    NULL, NULL
};

static jlif lnkStateIf = {
    "state", lnkState_alloc, lnkState_free,
    NULL, NULL, NULL, NULL, lnkState_string,
    NULL, NULL, NULL,
    NULL, NULL,
    NULL, lnkState_get_lset,
    lnkState_report, NULL, NULL
};
epicsExportAddress(jlif, lnkStateIf);
