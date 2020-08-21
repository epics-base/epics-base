/*************************************************************************\
* Copyright (c) 2016 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* lnkConst.c */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "dbDefs.h"
#include "errlog.h"
#include "epicsAssert.h"
#include "epicsString.h"
#include "epicsTypes.h"
#include "dbAccessDefs.h"
#include "dbConvertFast.h"
#include "dbLink.h"
#include "dbJLink.h"
#include "epicsExport.h"


typedef long (*FASTCONVERT)();

typedef struct const_link {
    jlink jlink;        /* embedded object */
    int nElems;
    enum {s0, si64, sf64, sc40, a0, ai64, af64, ac40} type;
    union {
        epicsInt64 scalar_integer;  /* si64 */
        epicsFloat64 scalar_double; /* sf64 */
        char *scalar_string;        /* sc40 */
        void *pmem;
        epicsInt64 *pintegers;      /* ai64 */
        epicsFloat64 *pdoubles;     /* af64 */
        char **pstrings;            /* ac40 */
    } value;
} const_link;

static lset lnkConst_lset;


/*************************** jlif Routines **************************/

static jlink* lnkConst_alloc(short dbfType)
{
    const_link *clink;

    if (dbfType != DBF_INLINK) {
        errlogPrintf("lnkConst: Only works with input links\n");
        return NULL;
    }

    clink = calloc(1, sizeof(*clink));
    if (!clink) {
        errlogPrintf("lnkConst: calloc() failed.\n");
        return NULL;
    }

    clink->type = s0;
    clink->nElems = 0;
    clink->value.pmem = NULL;

    return &clink->jlink;
}

static void lnkConst_free(jlink *pjlink)
{
    const_link *clink = CONTAINER(pjlink, const_link, jlink);

    switch (clink->type) {
        int i;
    case ac40:
        for (i=0; i<clink->nElems; i++)
            free(clink->value.pstrings[i]);
        /* fall through */
    case sc40:
    case ai64:
    case af64:
        free(clink->value.pmem);
        break;
    case s0:
    case a0:
    case si64:
    case sf64:
        break;
    }
    free(clink);
}

static jlif_result lnkConst_integer(jlink *pjlink, long long num)
{
    const_link *clink = CONTAINER(pjlink, const_link, jlink);
    int newElems = clink->nElems + 1;

    switch (clink->type) {
        void *buf;

    case s0:
        clink->type = si64;
        clink->value.scalar_integer = num;
        if (pjlink->debug)
            printf("   si64 := %lld\n", num);
        break;

    case a0:
        clink->type = ai64;
        /* fall through */
    case ai64:
        buf = realloc(clink->value.pmem, newElems * sizeof(epicsInt64));
        if (!buf)
            return jlif_stop;

        clink->value.pmem = buf;
        clink->value.pintegers[clink->nElems] = num;
        if (pjlink->debug)
            printf("   ai64 += %lld\n", num);
        break;

    case af64:
        buf = realloc(clink->value.pmem, newElems * sizeof(epicsFloat64));
        if (!buf)
            return jlif_stop;

        clink->value.pmem = buf;
        clink->value.pdoubles[clink->nElems] = num;
        if (pjlink->debug)
            printf("   af64 += %lld\n", num);
        break;

    case ac40:
        errlogPrintf("lnkConst: Mixed data types in array\n");
        /* fall through */
    default:
        return jlif_stop;
    }

    clink->nElems = newElems;
    return jlif_continue;
}

static jlif_result lnkConst_boolean(jlink *pjlink, int val)
{
    return lnkConst_integer(pjlink, val);
}

static jlif_result lnkConst_double(jlink *pjlink, double num)
{
    const_link *clink = CONTAINER(pjlink, const_link, jlink);
    int newElems = clink->nElems + 1;

    switch (clink->type) {
        epicsFloat64 *f64buf;
        int i;

    case s0:
        clink->type = sf64;
        clink->value.scalar_double = num;
        break;

    case a0:
        clink->type = af64;
        /* fall through */
    case af64:
        f64buf = realloc(clink->value.pmem, newElems * sizeof(epicsFloat64));
        if (!f64buf)
            return jlif_stop;

        f64buf[clink->nElems] = num;
        clink->value.pdoubles = f64buf;
        break;

    case ai64: /* promote earlier ai64 values to af64 */
        f64buf = calloc(newElems, sizeof(epicsFloat64));
        if (!f64buf)
            return jlif_stop;

        for (i = 0; i < clink->nElems; i++) {
            f64buf[i] = clink->value.pintegers[i];
        }
        free(clink->value.pmem);
        f64buf[clink->nElems] = num;
        clink->type = af64;
        clink->value.pdoubles = f64buf;
        break;

    case ac40:
        errlogPrintf("lnkConst: Mixed data types in array\n");
        /* fall through */
    default:
        return jlif_stop;
    }

    clink->nElems = newElems;
    return jlif_continue;
}

static jlif_result lnkConst_string(jlink *pjlink, const char *val, size_t len)
{
    const_link *clink = CONTAINER(pjlink, const_link, jlink);
    int newElems = clink->nElems + 1;

    switch (clink->type) {
        char **vec, *str;

    case s0:
        str = malloc(len+1);
        if (!str)
            return jlif_stop;

        strncpy(str, val, len);
        str[len] = '\0';
        clink->type = sc40;
        clink->value.scalar_string = str;
        break;

    case a0:
        clink->type = ac40;
        /* fall thorough */
    case ac40:
        vec = realloc(clink->value.pmem, newElems * sizeof(char *));
        if (!vec)
            return jlif_stop;
        str = malloc(len+1);
        if (!str)
            return jlif_stop;

        strncpy(str, val, len);
        str[len] = '\0';
        vec[clink->nElems] = str;
        clink->value.pstrings = vec;
        break;

    case af64:
    case ai64:
        errlogPrintf("lnkConst: Mixed data types in array\n");
        /* fall thorough */
    default:
        return jlif_stop;
    }

    clink->nElems = newElems;
    return jlif_continue;
}

static jlif_result lnkConst_start_array(jlink *pjlink)
{
    const_link *clink = CONTAINER(pjlink, const_link, jlink);

    if (clink->type != s0) {
        errlogPrintf("lnkConst: Embedded array value\n");
        return jlif_stop;
    }

    clink->type = a0;
    return jlif_continue;
}

static jlif_result lnkConst_end_array(jlink *pjlink)
{
    return jlif_continue;
}

static struct lset* lnkConst_get_lset(const jlink *pjlink)
{
    return &lnkConst_lset;
}


/* Report outputs:
 *    'const': integer 21
 *    'const': double 5.23
 *    'const': string "something"
 *    'const': array of 999 integers
 *        [1, 2, 3]
 *    'const': array of 1 double
 *        [1.2345]
 *    'const': array of 2 strings
 *        ["hello", "world"]
 *
 * Array values are only printed at level 2
 * because there might be quite a few of them.
 */

static void lnkConst_report(const jlink *pjlink, int level, int indent)
{
    const_link *clink = CONTAINER(pjlink, const_link, jlink);
    const char * const type_names[4] = {
        "bug", "integer", "double", "string"
    };
    const char * const dtype = type_names[clink->type & 3];

    if (clink->type > a0) {
        const char * const plural = clink->nElems > 1 ? "s" : "";

        printf("%*s'const': array of %d %s%s", indent, "",
            clink->nElems, dtype, plural);

        if (level < 2) {
            putchar('\n');
        }
        else {
            int i;

            switch (clink->type) {
            case ai64:
                printf("\n%*s[%lld", indent+2, "", clink->value.pintegers[0]);
                for (i = 1; i < clink->nElems; i++) {
                    printf(", %lld", clink->value.pintegers[i]);
                }
                break;
            case af64:
                printf("\n%*s[%g", indent+2, "", clink->value.pdoubles[0]);
                for (i = 1; i < clink->nElems; i++) {
                    printf(", %g", clink->value.pdoubles[i]);
                }
                break;
            case ac40:
                printf("\n%*s[\"%s\"", indent+2, "", clink->value.pstrings[0]);
                for (i = 1; i < clink->nElems; i++) {
                    printf(", \"%s\"", clink->value.pstrings[i]);
                }
                break;
            default:
                break;
            }
            printf("]\n");
        }
        return;
    }

    printf("%*s'const': %s", indent, "", dtype);

    switch (clink->type) {
    case si64:
        printf(" %lld\n", clink->value.scalar_integer);
        return;
    case sf64:
        printf(" %g\n", clink->value.scalar_double);
        return;
    case sc40:
        printf(" \"%s\"\n", clink->value.scalar_string);
        return;
    default:
        printf(" -- type=%d\n", clink->type);
        return;
    }
}

/*************************** lset Routines **************************/

static void lnkConst_remove(struct dbLocker *locker, struct link *plink)
{
    lnkConst_free(plink->value.json.jlink);
}

static long lnkConst_loadScalar(struct link *plink, short dbrType, void *pbuffer)
{
    const_link *clink = CONTAINER(plink->value.json.jlink, const_link, jlink);
    long status;

    if(INVALID_DB_REQ(dbrType))
        return S_db_badDbrtype;

    switch (clink->type) {
    case si64:
        if (clink->jlink.debug)
            printf("   si64 %lld\n", clink->value.scalar_integer);
        status = dbFastPutConvertRoutine[DBF_INT64][dbrType]
            (&clink->value.scalar_integer, pbuffer, NULL);
        break;

    case sf64:
        if (clink->jlink.debug)
            printf("   sf64 %g\n", clink->value.scalar_double);
        status = dbFastPutConvertRoutine[DBF_DOUBLE][dbrType]
            (&clink->value.scalar_double, pbuffer, NULL);
        break;

    case sc40:
        if (clink->jlink.debug)
            printf("   sc40 '%s'\n", clink->value.scalar_string);
        status = dbFastPutConvertRoutine[DBF_STRING][dbrType]
            (clink->value.scalar_string, pbuffer, NULL);
        break;

    case ai64:
        if (clink->jlink.debug)
            printf("   ai64 [%lld, ...]\n", clink->value.pintegers[0]);
        status = dbFastPutConvertRoutine[DBF_INT64][dbrType]
            (clink->value.pintegers, pbuffer, NULL);
        break;

    case af64:
        if (clink->jlink.debug)
            printf("   af64 [%g, ...]\n", clink->value.pdoubles[0]);
        status = dbFastPutConvertRoutine[DBF_DOUBLE][dbrType]
            (clink->value.pdoubles, pbuffer, NULL);
        break;

    case ac40:
        if (clink->jlink.debug)
            printf("   ac40 ['%s', ...]\n", clink->value.pstrings[0]);
        status = dbFastPutConvertRoutine[DBF_STRING][dbrType]
            (clink->value.pstrings[0], pbuffer, NULL);
        break;

    default:
        if (clink->jlink.debug)
            printf("   Bad type %d\n", clink->type);
        status = S_db_badField;
        break;
    }

    return status;
}

static long lnkConst_loadLS(struct link *plink, char *pbuffer, epicsUInt32 size,
    epicsUInt32 *plen)
{
    const_link *clink = CONTAINER(plink->value.json.jlink, const_link, jlink);
    const char *pstr;

    if(!size) return 0;

    switch (clink->type) {
    case sc40:
        if (clink->jlink.debug)
            printf("   sc40 '%s'\n", clink->value.scalar_string);
        pstr = clink->value.scalar_string;
        break;

    case ac40:
        if (clink->jlink.debug)
            printf("   ac40 ['%s', ...]\n", clink->value.pstrings[0]);
        pstr = clink->value.pstrings[0];
        break;

    default:
        if (clink->jlink.debug)
            printf("   Bad type %d\n", clink->type);
        return S_db_badField;
    }

    strncpy(pbuffer, pstr, --size);
    pbuffer[size] = 0;
    *plen = (epicsUInt32) strlen(pbuffer) + 1;
    return 0;
}

static long lnkConst_loadArray(struct link *plink, short dbrType, void *pbuffer,
        long *pnReq)
{
    const_link *clink = CONTAINER(plink->value.json.jlink, const_link, jlink);
    short dbrSize;
    char *pdest = pbuffer;
    int nElems = clink->nElems;
    FASTCONVERT conv;
    long status;

    if(INVALID_DB_REQ(dbrType))
        return S_db_badDbrtype;

    dbrSize = dbValueSize(dbrType);

    if (nElems > *pnReq)
        nElems = *pnReq;

    switch (clink->type) {
        int i;

    case si64:
        if (clink->jlink.debug)
            printf("   si64 %lld\n", clink->value.scalar_integer);
        status = dbFastPutConvertRoutine[DBF_INT64][dbrType]
            (&clink->value.scalar_integer, pdest, NULL);
        break;

    case sf64:
        if (clink->jlink.debug)
            printf("   sf64 %g\n", clink->value.scalar_double);
        status = dbFastPutConvertRoutine[DBF_DOUBLE][dbrType]
            (&clink->value.scalar_double, pdest, NULL);
        break;

    case sc40:
        if (clink->jlink.debug)
            printf("   sc40 '%s'\n", clink->value.scalar_string);
        if (dbrType != DBF_CHAR) {
            status = dbFastPutConvertRoutine[DBF_STRING][dbrType]
                (clink->value.scalar_string, pbuffer, NULL);
        }
        else {
            /* Long string conversion */
            strncpy(pbuffer, clink->value.scalar_string, *pnReq);
            ((char *)pbuffer)[*pnReq] = 0;
            nElems = strlen(pbuffer) + 1;
            status = 0;
        }
        break;

    case ai64:
        if (clink->jlink.debug)
            printf("   ai64 [%lld, ...]\n", clink->value.pintegers[0]);
        conv = dbFastPutConvertRoutine[DBF_INT64][dbrType];
        for (i = 0; i < nElems; i++) {
            conv(&clink->value.pintegers[i], pdest, NULL);
            pdest += dbrSize;
        }
        status = 0;
        break;

    case af64:
        if (clink->jlink.debug)
            printf("   af64 [%g, ...]\n", clink->value.pdoubles[0]);
        conv = dbFastPutConvertRoutine[DBF_DOUBLE][dbrType];
        for (i = 0; i < nElems; i++) {
            conv(&clink->value.pdoubles[i], pdest, NULL);
            pdest += dbrSize;
        }
        status = 0;
        break;

    case ac40:
        if (clink->jlink.debug)
            printf("   ac40 ['%s', ...]\n", clink->value.pstrings[0]);
        conv = dbFastPutConvertRoutine[DBF_STRING][dbrType];
        for (i = 0; i < nElems; i++) {
            conv(clink->value.pstrings[i], pdest, NULL);
            pdest += dbrSize;
        }
        status = 0;
        break;

    default:
        if (clink->jlink.debug)
            printf("   Bad type %d\n", clink->type);
        status = S_db_badField;
    }
    *pnReq = nElems;
    return status;
}

static long lnkConst_getNelements(const struct link *plink, long *nelements)
{
    *nelements = 0;
    return 0;
}

static long lnkConst_getValue(struct link *plink, short dbrType, void *pbuffer,
        long *pnRequest)
{
    if (pnRequest)
        *pnRequest = 0;
    return 0;
}

static long lnkConst_putValue(struct link *plink, short dbrType,
            const void *pbuffer, long nRequest)
{
    return 0;
}


/************************* Interface Tables *************************/

static lset lnkConst_lset = {
    1, 0, /* Constant, not Volatile */
    NULL, lnkConst_remove,
    lnkConst_loadScalar, lnkConst_loadLS, lnkConst_loadArray, NULL,
    NULL, lnkConst_getNelements, lnkConst_getValue,
    NULL, NULL, NULL,
    NULL, NULL,
    NULL, NULL,
    lnkConst_putValue, NULL,
    NULL, NULL
};

static jlif lnkConstIf = {
    "const", lnkConst_alloc, lnkConst_free,
    NULL, lnkConst_boolean, lnkConst_integer, lnkConst_double, lnkConst_string,
    NULL, NULL, NULL,
    lnkConst_start_array, lnkConst_end_array,
    NULL, lnkConst_get_lset,
    lnkConst_report, NULL, NULL
};
epicsExportAddress(jlif, lnkConstIf);
