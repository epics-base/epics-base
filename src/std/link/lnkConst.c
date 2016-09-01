/*************************************************************************\
* Copyright (c) 2016 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
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
#include "epicsTypes.h"
#include "dbAccessDefs.h"
#include "dbConvertFast.h"
#include "dbLink.h"
#include "dbJLink.h"
#include "epicsExport.h"


/* Change 'undef' to 'define' to turn on debug statements: */
#undef DEBUG_LINK

#ifdef DEBUG_LINK
    int lnkConstDebug = 10;
#   define IFDEBUG(n) \
        if (lnkConstDebug >= n) /* block or statement */
#else
#   define IFDEBUG(n) \
        if(0) /* Compiler will elide the block or statement */
#endif

typedef long (*FASTCONVERT)();

typedef struct clink {
    jlink jlink;        /* embedded object */
    int nElems;
    enum {s0, si32, sf64, sc40, a0, ai32, af64, ac40} type;
    union {
        epicsInt32 scalar_integer;
        epicsFloat64 scalar_double;
        char * scalar_string;
        void *pmem;
        epicsInt32 *pintegers;
        epicsFloat64 *pdoubles;
        char **pstrings;
    } value;
} clink;

static lset lnkConst_lset;


/*************************** jlif Routines **************************/

static jlink* lnkConst_alloc(short dbfType) {
    clink *clink = calloc(1, sizeof(struct clink));

    IFDEBUG(10)
        printf("lnkConst_alloc()\n");

    clink->type = s0;
    clink->nElems = 0;
    clink->value.pmem = NULL;

    IFDEBUG(10)
        printf("lnkConst_alloc -> const@%p\n", clink);

    return &clink->jlink;
}

static void lnkConst_free(jlink *pjlink) {
    clink *clink = CONTAINER(pjlink, struct clink, jlink);

    IFDEBUG(10)
        printf("lnkConst_free(const@%p)\n", pjlink);

    switch (clink->type) {
        int i;
    case ac40:
        for (i=0; i<clink->nElems; i++)
            free(clink->value.pstrings[i]);
        /* fall through */
    case ai32:
    case af64:
        free(clink->value.pmem);
    default:
        break;
    }
    free(clink);
}

static jlif_result lnkConst_integer(jlink *pjlink, long num) {
    clink *clink = CONTAINER(pjlink, struct clink, jlink);

    IFDEBUG(10)
        printf("lnkConst_integer(const@%p, %ld)\n", pjlink, num);

    switch (clink->type) {
        void *buf;
        int nElems;

    case s0:
        clink->nElems = 1;
        clink->type = si32;
        clink->value.scalar_integer = num;
        break;

    case a0:
        clink->type = ai32;
        /* fall thorough */
    case ai32:
        nElems = clink->nElems + 1;
        buf = realloc(clink->value.pmem, nElems * sizeof(epicsInt32));
        if (!buf) break;
        clink->value.pmem = buf;
        clink->value.pintegers[clink->nElems] = num;
        clink->nElems = nElems;
        break;

    case af64:
        nElems = clink->nElems + 1;
        buf = realloc(clink->value.pmem, nElems * sizeof(epicsFloat64));
        if (!buf) break;
        clink->value.pmem = buf;
        clink->value.pdoubles[clink->nElems++] = num;
        break;

    case ac40:
        errlogPrintf("lnkConst: Mixed data types in array\n");
        /* FIXME ??? */
    default:
        return jlif_stop;
    }
    return jlif_continue;
}

static jlif_result lnkConst_boolean(jlink *pjlink, int val) {
    IFDEBUG(10)
        printf("lnkConst_boolean(const@%p, %d)\n", pjlink, val);

    return lnkConst_integer(pjlink, val);
}

static jlif_result lnkConst_double(jlink *pjlink, double num) {
    clink *clink = CONTAINER(pjlink, struct clink, jlink);

    IFDEBUG(10)
        printf("lnkConst_double(const@%p, %g)\n", pjlink, num);

    switch (clink->type) {
        epicsFloat64 *f64buf;
        int nElems, i;

    case s0:
        clink->nElems = 1;
        clink->type = sf64;
        clink->value.scalar_double = num;
        break;

    case a0:
        clink->type = af64;
        /* fall thorough */
    case af64:
        nElems = clink->nElems + 1;
        f64buf = realloc(clink->value.pmem, nElems * sizeof(epicsFloat64));
        if (!f64buf) break;
        clink->value.pdoubles = f64buf;
        clink->value.pdoubles[clink->nElems++] = num;
        break;

    case ai32: /* promote earlier ai32 values to af64 */
        f64buf = calloc(clink->nElems + 1, sizeof(epicsFloat64));
        if (!f64buf) break;
        for (i = 0; i < clink->nElems; i++) {
            f64buf[i] = clink->value.pintegers[i];
        }
        f64buf[clink->nElems++] = num;
        free(clink->value.pmem);
        clink->value.pdoubles = f64buf;
        clink->type = af64;
        break;

    case ac40:
        errlogPrintf("lnkConst: Mixed data types in array\n");
        /* FIXME ??? */
    default:
        return jlif_stop;
    }

    return jlif_continue;
}

static jlif_result lnkConst_string(jlink *pjlink, const char *val, size_t len) {
    clink *clink = CONTAINER(pjlink, struct clink, jlink);

    IFDEBUG(10)
        printf("lnkConst_string(const@%p, \"%.*s\")\n", clink, (int) len, val);

    switch (clink->type) {
        char *buf, **vec;
        int nElems;

    case s0:
        buf = malloc(len + 1);
        if (!buf)
            return jlif_stop;

        strncpy(buf, val, len);
        buf[len] = '\0';

        clink->nElems = 1;
        clink->type = sc40;
        clink->value.scalar_string = buf;
        break;

    case a0:
        clink->type = ac40;
        /* fall thorough */
    case ac40:
        if (len > MAX_STRING_SIZE)
            len = MAX_STRING_SIZE;

        buf = malloc(len + 1);
        if (!buf)
            return jlif_stop;

        strncpy(buf, val, len);
        buf[len] = '\0';

        nElems = clink->nElems + 1;

        vec = realloc(clink->value.pmem, nElems * sizeof(char *));
        if (!vec) {
            free(buf);
            break;
        }

        vec[clink->nElems++] = buf;
        clink->value.pstrings = vec;
        break;

    case af64:
    case ai32:
        errlogPrintf("lnkConst: Mixed data types in array\n");
        /* fall thorough */
    default:
        return jlif_stop;
    }

    return jlif_continue;
}

static jlif_result lnkConst_start_array(jlink *pjlink) {
    clink *clink = CONTAINER(pjlink, struct clink, jlink);

    IFDEBUG(10)
        printf("lnkConst_start_array(const@%p)\n", pjlink);

    if (clink->type != s0) {
        errlogPrintf("lnkConst: Embedded array value\n");
        return jlif_stop;
    }

    clink->type = a0;
    return jlif_continue;
}

static jlif_result lnkConst_end_array(jlink *pjlink) {
    IFDEBUG(10)
        printf("lnkConst_end_array(const@%p)\n", pjlink);

    return jlif_continue;
}

static struct lset* lnkConst_get_lset(const jlink *pjlink, struct link *plink) {
    IFDEBUG(10)
        printf("lnkConst_get_lset(const@%p, %p)\n", pjlink, plink);

    return &lnkConst_lset;
}

static void lnkConst_report(const jlink *pjlink) {
    clink *clink = CONTAINER(pjlink, struct clink, jlink);

    IFDEBUG(10)
        printf("lnkConst_report(const@%p)\n", clink);

    /* FIXME Implement! */
}

/*************************** lset Routines **************************/

static long lnkConst_loadScalar(struct link *plink, short dbrType, void *pbuffer)
{
    clink *clink = CONTAINER(plink->value.json.jlink, struct clink, jlink);
    long status;

    IFDEBUG(10)
        printf("lnkConst_loadScalar(const@%p, %d, %p)\n",
            clink, dbrType, pbuffer);

    switch (clink->type) {
    case si32:
        status = dbFastPutConvertRoutine[DBF_LONG][dbrType]
            (&clink->value.scalar_integer, pbuffer, NULL);
        break;

    case sf64:
        status = dbFastPutConvertRoutine[DBF_DOUBLE][dbrType]
            (&clink->value.scalar_double, pbuffer, NULL);
        break;

    case sc40:
        status = dbFastPutConvertRoutine[DBF_STRING][dbrType]
            (clink->value.scalar_string, pbuffer, NULL);
        break;

    case ai32:
        status = dbFastPutConvertRoutine[DBF_LONG][dbrType]
            (clink->value.pintegers, pbuffer, NULL);
        break;

    case af64:
        status = dbFastPutConvertRoutine[DBF_DOUBLE][dbrType]
            (clink->value.pdoubles, pbuffer, NULL);
        break;

    case ac40:
        status = dbFastPutConvertRoutine[DBF_STRING][dbrType]
            (clink->value.pstrings[0], pbuffer, NULL);
        break;

    default:
        status = S_db_badField;
        break;
    }

    return status;
}

static long lnkConst_loadLS(struct link *plink, char *pbuffer, epicsUInt32 size,
    epicsUInt32 *plen)
{
    clink *clink = CONTAINER(plink->value.json.jlink, struct clink, jlink);
    const char *pstr;

    IFDEBUG(10)
        printf("lnkConst_loadLS(const@%p, %p, %d, %d)\n",
            clink, pbuffer, size, *plen);

    switch (clink->type) {
    case sc40:
        pstr = clink->value.scalar_string;
        break;

    case ac40:
        pstr = clink->value.pstrings[0];
        break;

    default:
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
    clink *clink = CONTAINER(plink->value.json.jlink, struct clink, jlink);
    short dbrSize = dbValueSize(dbrType);
    char *pdest = pbuffer;
    int nElems = clink->nElems;
    FASTCONVERT conv;
    long status;

    IFDEBUG(10)
        printf("lnkConst_loadArray(const@%p, %d, %p, (%ld))\n",
            clink, dbrType, pbuffer, *pnReq);

    if (nElems > *pnReq)
        nElems = *pnReq;

    switch (clink->type) {
        int i;

    case si32:
        status = dbFastPutConvertRoutine[DBF_LONG][dbrType]
            (&clink->value.scalar_integer, pdest, NULL);
        break;

    case sf64:
        status = dbFastPutConvertRoutine[DBF_DOUBLE][dbrType]
            (&clink->value.scalar_double, pdest, NULL);
        break;

    case sc40:
        status = dbFastPutConvertRoutine[DBF_STRING][dbrType]
            (clink->value.scalar_string, pbuffer, NULL);
        break;

    case ai32:
        conv = dbFastPutConvertRoutine[DBF_LONG][dbrType];
        for (i = 0; i < nElems; i++) {
            conv(&clink->value.pintegers[i], pdest, NULL);
            pdest += dbrSize;
        }
        status = 0;
        break;

    case af64:
        conv = dbFastPutConvertRoutine[DBF_DOUBLE][dbrType];
        for (i = 0; i < nElems; i++) {
            conv(&clink->value.pdoubles[i], pdest, NULL);
            pdest += dbrSize;
        }
        status = 0;
        break;

    case ac40:
        conv = dbFastPutConvertRoutine[DBF_STRING][dbrType];
        for (i = 0; i < nElems; i++) {
            conv(clink->value.pstrings[i], pdest, NULL);
            pdest += dbrSize;
        }
        status = 0;
        break;

    default:
        status = S_db_badField;
    }
    *pnReq = nElems;
    return status;
}

static long lnkConst_getNelements(const struct link *plink, long *nelements)
{
    IFDEBUG(10)
        printf("lnkConst_getNelements(const@%p, (%ld))\n",
            plink->value.json.jlink, *nelements);

    *nelements = 0;
    return 0;
}

static long lnkConst_getValue(struct link *plink, short dbrType, void *pbuffer,
        epicsEnum16 *pstat, epicsEnum16 *psevr, long *pnRequest)
{
    IFDEBUG(10)
        printf("lnkConst_loadScalar(const@%p, %d, %p, ... (%ld))\n",
            plink->value.json.jlink, dbrType, pbuffer, *pnRequest);

    if (pnRequest)
        *pnRequest = 0;
    return 0;
}


/************************* Interface Tables *************************/

static lset lnkConst_lset = {
    1, 0, /* Constant, not Volatile */
    NULL, lnkConst_loadScalar, lnkConst_loadLS, lnkConst_loadArray, NULL,
    NULL, lnkConst_getNelements, lnkConst_getValue,
    NULL, NULL, NULL,
    NULL, NULL,
    NULL, NULL,
    NULL, NULL,
    NULL
};

static jlif lnkConstIf = {
    "const", lnkConst_alloc, lnkConst_free, NULL,
    NULL, lnkConst_boolean, lnkConst_integer, lnkConst_double, lnkConst_string,
    NULL, NULL, NULL, lnkConst_start_array, lnkConst_end_array,
    NULL, lnkConst_get_lset, lnkConst_report
};
epicsExportAddress(jlif, lnkConstIf);

