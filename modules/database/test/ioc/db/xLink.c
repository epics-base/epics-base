/*************************************************************************\
* Copyright (c) 2016 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* xLink.c */

#include "dbDefs.h"
#include "dbLink.h"
#include "dbJLink.h"
#include "epicsExport.h"


typedef struct xlink {
    jlink jlink;        /* embedded object */
    /* ... */
} xlink;

static lset xlink_lset;

static jlink* xlink_alloc(short dbfType)
{
    xlink *xlink = calloc(1, sizeof(struct xlink));

    return &xlink->jlink;
}

static void xlink_free(jlink *pjlink)
{
    xlink *xlink = CONTAINER(pjlink, struct xlink, jlink);

    free(xlink);
}

static jlif_result xlink_boolean(jlink *pjlink, int val)
{
    return val; /* False triggers a parse failure */
}

static struct lset* xlink_get_lset(const jlink *pjlink)
{
    return &xlink_lset;
}


static void xlink_remove(struct dbLocker *locker, struct link *plink)
{
    xlink_free(plink->value.json.jlink);
}

static long xlink_getNelements(const struct link *plink, long *nelements)
{
    *nelements = 0;
    return 0;
}

static long xlink_getValue(struct link *plink, short dbrType, void *pbuffer,
        long *pnRequest)
{
    if (pnRequest)
        *pnRequest = 0;
    return 0;
}


static lset xlink_lset = {
    1, 0, /* Constant, not Volatile */
    NULL, xlink_remove,
    NULL, NULL, NULL, NULL,
    NULL, xlink_getNelements, xlink_getValue,
    NULL, NULL, NULL,
    NULL, NULL,
    NULL, NULL,
    NULL, NULL,
    NULL, NULL
};

static jlif xlinkIf = {
    "x", xlink_alloc, xlink_free,
    NULL, xlink_boolean, NULL, NULL, NULL,
    NULL, NULL, NULL,
    NULL, NULL,
    NULL, xlink_get_lset,
    NULL, NULL
};
epicsExportAddress(jlif, xlinkIf);

