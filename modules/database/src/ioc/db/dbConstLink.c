/*************************************************************************\
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
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
#include "epicsStdlib.h"

#include "dbAccessDefs.h"
#include "dbAddr.h"
#include "dbCommon.h"
#include "dbConstLink.h"
#include "dbConvertJSON.h"
#include "dbFldTypes.h"
#include "dbLink.h"
#include "link.h"
#include "errlog.h"

/**************************** Convert functions ****************************/

/* The difference between these and dbFastConvert is that constants
 * may contain hex numbers, whereas database conversions can't.
 */

/* Copy to STRING */
static long cvt_st_st(const char *from, void *pfield, const dbAddr *paddr)
{
    char *to = pfield;
    size_t size;

    if (paddr && paddr->field_size < MAX_STRING_SIZE) {
        size = paddr->field_size - 1;
    } else {
        size = MAX_STRING_SIZE - 1;
    }
    strncpy(to, from, size);
    to[size] = 0;
    return 0;
}

/* Most integer conversions are identical */
#define cvt_st_int(TYPE) static long \
cvt_st_ ## TYPE(const char *from, void *pfield, const dbAddr *paddr) { \
    epics##TYPE *to = pfield; \
    char *end; \
\
    if (*from == 0) { \
        *to = 0; \
        return 0; \
    } \
    return epicsParse##TYPE(from, to, 0, &end); \
}

/* Instantiate for CHAR, UCHAR, SHORT, USHORT and LONG */
cvt_st_int(Int8)
cvt_st_int(UInt8)
cvt_st_int(Int16)
cvt_st_int(UInt16)
cvt_st_int(Int32)

/* Conversion for ULONG is different... */
static long cvt_st_UInt32(const char *from, void *pfield, const dbAddr *paddr)
{
    epicsUInt32 *to = pfield;
    char *end;
    long status;

    if (*from == 0) {
       *to = 0;
       return 0;
    }
    status = epicsParseUInt32(from, to, 0, &end);
    if (status == S_stdlib_noConversion ||
        (!status && (*end == '.' || *end == 'e' || *end == 'E'))) {
        /*
         * Convert via double so numbers like 1.0e3 convert properly.
         * db_access pretends ULONG fields are DOUBLE.
         */
        double dval;

        status = epicsParseFloat64(from, &dval, &end);
        if (!status &&
            dval >=0 &&
            dval <= ULONG_MAX)
            *to = dval;
    }
    return status;
}

/* Instantiate for INT64 and UINT64 */
cvt_st_int(Int64)
cvt_st_int(UInt64)


/* Float conversions are identical */
#define cvt_st_float(TYPE) static long \
cvt_st_ ## TYPE(const char *from, void *pfield, const dbAddr *paddr) { \
    epics##TYPE *to = pfield; \
    char *end; \
\
    if (*from == 0) { \
        *to = 0; \
        return 0; \
    } \
    return epicsParse##TYPE(from, to, &end); \
}

/* Instantiate for FLOAT32 and FLOAT64 */
cvt_st_float(Float32)
cvt_st_float(Float64)


static long (*convert[DBF_DOUBLE+1])(const char *, void *, const dbAddr *) = {
    cvt_st_st,
    cvt_st_Int8,    cvt_st_UInt8,
    cvt_st_Int16,   cvt_st_UInt16,
    cvt_st_Int32,   cvt_st_UInt32,
    cvt_st_Int64,   cvt_st_UInt64,
    cvt_st_Float32, cvt_st_Float64
};

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

    if (!pstr || !pstr[0])
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

    if(dbrType>=NELEMENTS(convert))
        return S_db_badDbrtype;

    return convert[dbrType](pstr, pbuffer, NULL);
}

static long dbConstLoadLS(struct link *plink, char *pbuffer, epicsUInt32 size,
    epicsUInt32 *plen)
{
    const char *pstr = plink->value.constantStr;
    long status;

    if (!pstr || !pstr[0])
        return S_db_badField;

    status = dbLSConvertJSON(pstr, pbuffer, size, plen);
    if (status)
        errlogPrintf("... while parsing link %s.%s\n",
            plink->precord->name, dbLinkFieldName(plink));
    return status;
}

static long dbConstLoadArray(struct link *plink, short dbrType, void *pbuffer,
        long *pnReq)
{
    const char *pstr = plink->value.constantStr;
    long status;

    if (!pstr || !pstr[0])
        return S_db_badField;

    /* Choice values must be numeric */
    if (dbrType == DBF_MENU || dbrType == DBF_ENUM || dbrType == DBF_DEVICE)
        dbrType = DBF_USHORT;

    status = dbPutConvertJSON(pstr, dbrType, pbuffer, pnReq);
    if (status)
        errlogPrintf("... while parsing link %s.%s\n",
            plink->precord->name, dbLinkFieldName(plink));
    return status;
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

static long dbConstPutValue(struct link *plink, short dbrType,
            const void *pbuffer, long nRequest)
{
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
    dbConstPutValue, NULL,
    NULL, NULL
};
