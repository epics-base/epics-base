/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* dbConvert.c */
/*
 *      Original Author: Bob Dalesio
 *      Date:            11-7-90
*/

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "cvtFast.h"
#include "dbDefs.h"
#include "epicsConvert.h"
#include "epicsStdlib.h"
#include "errlog.h"
#include "errMdef.h"

#define epicsExportSharedSymbols
#include "dbAccessDefs.h"
#include "dbAddr.h"
#include "dbBase.h"
#include "dbConvert.h"
#include "dbFldTypes.h"
#include "dbStaticLib.h"
#include "link.h"
#include "recGbl.h"
#include "recSup.h"

/* Helper for copy as bytes with no type conversion.
 * Assumes nRequest <= no_bytes
 * nRequest, no_bytes, and offset should be given in bytes.
 */
static void copyNoConvert(const void *pfrom,
    void *pto, long nRequest, long no_bytes, long offset)
{
    const void *pfrom_offset = (const char *) pfrom + offset;

    if (offset > 0 && offset < no_bytes && offset + nRequest > no_bytes) {
        const size_t N = no_bytes - offset;
        void *pto_N = (char *) pto + N;

        /* copy with wrap */
        memmove(pto,   pfrom_offset, N);
        memmove(pto_N, pfrom,        nRequest - N);
    } else {
        /* no wrap, just copy */
        memmove(pto, pfrom_offset, nRequest);
    }
}
#define COPYNOCONVERT(N, FROM, TO, NREQ, NO_ELEM, OFFSET) \
    copyNoConvert(FROM, TO, (N)*(NREQ), (N)*(NO_ELEM), (N)*(OFFSET))

#define GET(typea, typeb) (const dbAddr *paddr, \
    void *pto, long nRequest, long no_elements, long offset) \
{ \
    typea *psrc = (typea *) paddr->pfield; \
    typeb *pdst = (typeb *) pto; \
    \
    if (nRequest==1 && offset==0) { \
        *pdst = (typeb) *psrc; \
        return 0; \
    } \
    psrc += offset; \
    while (nRequest--) { \
        *pdst++ = (typeb) *psrc++; \
        if (++offset == no_elements) \
            psrc = (typea *) paddr->pfield; \
    } \
    return 0; \
}

#define GET_NOCONVERT(typea, typeb) (const dbAddr *paddr, \
    void *pto, long nRequest, long no_elements, long offset) \
{ \
    if (nRequest==1 && offset==0) { \
        typea *psrc = (typea *) paddr->pfield; \
        typeb *pdst = (typeb *) pto; \
        \
        *pdst = (typeb) *psrc; \
        return 0; \
    } \
    COPYNOCONVERT(sizeof(typeb), paddr->pfield, pto, nRequest, no_elements, offset); \
    return 0; \
}

#define PUT(typea, typeb) (dbAddr *paddr, \
    const void *pfrom, long nRequest, long no_elements, long offset) \
{ \
    const typea *psrc = (const typea *) pfrom; \
    typeb *pdst = (typeb *) paddr->pfield; \
    \
    if (nRequest==1 && offset==0) { \
        *pdst = (typeb) *psrc; \
        return 0; \
    } \
    pdst += offset; \
    while (nRequest--) { \
        *pdst++ = (typeb) *psrc++; \
        if (++offset == no_elements) \
            pdst = (typeb *) paddr->pfield; \
    } \
    return 0; \
}

#define PUT_NOCONVERT(typea, typeb) (dbAddr *paddr, \
    const void *pfrom, long nRequest, long no_elements, long offset) \
{ \
    if (nRequest==1 && offset==0) { \
        const typea *psrc = (const typea *) pfrom; \
        typeb *pdst = (typeb *) paddr->pfield; \
        \
        *pdst = (typeb) *psrc; \
        return 0; \
    } \
    COPYNOCONVERT(sizeof(typeb), pfrom, paddr->pfield, nRequest, no_elements, offset); \
    return 0; \
}


/* dbAccess Get conversion support routines */

static long getStringString(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    char *psrc = paddr->pfield;
    char *pdst = (char *) pto;
    short size = paddr->field_size;
    short sizeto;

    /* always force result string to be null terminated*/
    sizeto = size;
    if (sizeto >= MAX_STRING_SIZE)
        sizeto = MAX_STRING_SIZE - 1;

    if (nRequest==1 && offset==0) {
        strncpy(pdst, psrc, sizeto);
        pdst[sizeto] = 0;
        return 0;
    }
    psrc += size * offset;
    while (nRequest--) {
        strncpy(pdst, psrc, sizeto);
        pdst[sizeto] = 0;
        pdst += MAX_STRING_SIZE;
        if (++offset == no_elements)
            psrc = paddr->pfield;
        else
            psrc += size;
    }
    return 0;
}

static long getStringChar(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    char *psrc = (char *) paddr->pfield + MAX_STRING_SIZE * offset;
    epicsInt8 *pdst = pto;

    while (nRequest--) {
        if (*psrc == 0)
            *pdst++ = 0;
        else {
            char *end;
            long status = epicsParseInt8(psrc, pdst++, 10, &end);

            if (status)
                return status;
        }
        if (++offset == no_elements)
            psrc = paddr->pfield;
        else
            psrc += MAX_STRING_SIZE;
    }
    return 0;
}

static long getStringUchar(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    char *psrc = (char *) paddr->pfield + MAX_STRING_SIZE * offset;
    epicsUInt8 *pdst = pto;

    while (nRequest--) {
        if (*psrc == 0)
            *pdst++ = 0;
        else {
            char *end;
            long status = epicsParseUInt8(psrc, pdst++, 10, &end);

            if (status)
                return status;
        }
        if (++offset == no_elements)
            psrc = paddr->pfield;
        else
            psrc += MAX_STRING_SIZE;
    }
    return 0;
}

static long getStringShort(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    char *psrc = (char *) paddr->pfield + MAX_STRING_SIZE * offset;
    epicsInt16 *pdst = pto;

    while (nRequest--) {
        if (*psrc == 0)
            *pdst++ = 0;
        else {
            char *end;
            long status = epicsParseInt16(psrc, pdst++, 10, &end);

            if (status)
                return status;
        }
        if (++offset == no_elements)
            psrc = paddr->pfield;
        else
            psrc += MAX_STRING_SIZE;
    }
    return 0;
}

static long getStringUshort(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    char *psrc = (char *) paddr->pfield + MAX_STRING_SIZE * offset;
    epicsUInt16 *pdst = pto;

    while (nRequest--) {
        if (*psrc == 0)
            *pdst++ = 0;
        else {
            char *end;
            long status = epicsParseUInt16(psrc, pdst++, 10, &end);

            if (status)
                return status;
        }
        if (++offset == no_elements)
            psrc = paddr->pfield;
        else
            psrc += MAX_STRING_SIZE;
    }
    return 0;
}

static long getStringLong(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    char *psrc = (char *) paddr->pfield + MAX_STRING_SIZE * offset;
    epicsInt32 *pdst = pto;

    while (nRequest--) {
        if (*psrc == 0)
            *pdst++ = 0;
        else {
            char *end;
            long status = epicsParseInt32(psrc, pdst++, 10, &end);

            if (status)
                return status;
        }
        if (++offset == no_elements)
            psrc = paddr->pfield;
        else
            psrc += MAX_STRING_SIZE;
    }
    return 0;
}

static long getStringUlong(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    char *psrc = (char *) paddr->pfield + MAX_STRING_SIZE * offset;
    epicsUInt32 *pdst = pto;

    while (nRequest--) {
        if (*psrc == 0)
            *pdst++ = 0;
        else {
            char *end;
            long status = epicsParseUInt32(psrc, pdst, 10, &end);

            if (status == S_stdlib_noConversion ||
                (!status && (*end == '.' || *end == 'e' || *end == 'E'))) {
                /*
                 * Convert via double so numbers like 1.0e3 convert properly.
                 * db_access pretends unsigned long is double.
                 */
                epicsFloat64 dval;

                status = epicsParseFloat64(psrc, &dval, &end);
                if (!status && 0 <= dval && dval <= ULONG_MAX)
                    *pdst = dval;
            }
            if (status)
                return status;
            pdst++;
        }
        if (++offset == no_elements)
            psrc = paddr->pfield;
        else
            psrc += MAX_STRING_SIZE;
    }
    return 0;
}

static long getStringInt64(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    char *psrc = (char *) paddr->pfield + MAX_STRING_SIZE * offset;
    epicsInt64 *pdst = pto;

    while (nRequest--) {
        if (*psrc == 0)
            *pdst++ = 0;
        else {
            char *end;
            long status = epicsParseInt64(psrc, pdst++, 10, &end);

            if (status)
                return status;
        }
        if (++offset == no_elements)
            psrc = paddr->pfield;
        else
            psrc += MAX_STRING_SIZE;
    }
    return 0;
}

static long getStringUInt64(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    char *psrc = (char *) paddr->pfield + MAX_STRING_SIZE * offset;
    epicsUInt64 *pdst = pto;

    while (nRequest--) {
        if (*psrc == 0)
            *pdst++ = 0;
        else {
            char *end;
            long status = epicsParseUInt64(psrc, pdst++, 0, &end);

            if (status)
                return status;
        }
        if (++offset == no_elements)
            psrc = paddr->pfield;
        else
            psrc += MAX_STRING_SIZE;
    }
    return 0;
}

static long getStringFloat(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    char *psrc = (char *) paddr->pfield + MAX_STRING_SIZE * offset;
    epicsFloat32 *pdst = pto;

    while (nRequest--) {
        if (*psrc == 0)
            *pdst++ = 0;
        else {
            char *end;
            long status = epicsParseFloat32(psrc, pdst++, &end);

            if (status)
                return status;
        }
        if (++offset == no_elements)
            psrc = paddr->pfield;
        else
            psrc += MAX_STRING_SIZE;
    }
    return 0;
}

static long getStringDouble(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    char *psrc = (char *) paddr->pfield + MAX_STRING_SIZE * offset;
    epicsFloat64 *pdst = pto;

    while (nRequest--) {
        if (*psrc == 0)
            *pdst++ = 0;
        else {
            char *end;
            long status = epicsParseFloat64(psrc, pdst++, &end);

            if (status)
                return status;
        }
        if (++offset == no_elements)
            psrc = paddr->pfield;
        else
            psrc += MAX_STRING_SIZE;
    }
    return 0;
}


static long getCharString(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    char *psrc = (char *) paddr->pfield;
    char *pdst = (char *) pto;

    if (nRequest==1 && offset==0) {
        cvtCharToString(*psrc, pdst);
        return 0;
    }
    psrc += offset;
    while (nRequest--) {
        cvtCharToString(*psrc, pdst);
        pdst += MAX_STRING_SIZE;
        if (++offset == no_elements)
            psrc = (char *) paddr->pfield;
        else
            psrc++;
    }
    return 0;
}

static long getCharChar(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    char *psrc = (char *) paddr->pfield;
    char *pdst = (char *) pto;

    if (paddr->pfldDes && paddr->pfldDes->field_type == DBF_STRING) {
        /* This is a DBF_STRING field being read as a long string.
         * The buffer we return must be zero-terminated.
         */
        pdst[--nRequest] = 0;
        if (nRequest == 0)
            return 0;
    }
    if (nRequest==1 && offset==0) {
        *pdst = *psrc;
        return 0;
    }
    COPYNOCONVERT(sizeof(char), paddr->pfield, pto, nRequest, no_elements, offset);
    return 0;
}

static long getCharUchar(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    char *psrc = (char *) paddr->pfield;
    epicsUInt8 *pdst = (epicsUInt8 *) pto;

    if (paddr->pfldDes && paddr->pfldDes->field_type == DBF_STRING) {
        /* This is a DBF_STRING field being read as a long string.
         * The buffer we return must be zero-terminated.
         */
        pdst[--nRequest] = 0;
        if (nRequest == 0)
            return 0;
    }
    if (nRequest==1 && offset==0) {
        *pdst = *psrc;
        return 0;
    }
    COPYNOCONVERT(sizeof(char), paddr->pfield, pto, nRequest, no_elements, offset);
    return 0;
}

static long getCharShort GET(char, epicsInt16)
static long getCharUshort GET(char, epicsUInt16)
static long getCharLong GET(char, epicsInt32)
static long getCharUlong GET(char, epicsUInt32)
static long getCharInt64 GET(char, epicsInt64)
static long getCharUInt64 GET(char, epicsUInt64)
static long getCharFloat GET(char, epicsFloat32)
static long getCharDouble GET(char, epicsFloat64)
static long getCharEnum GET(char, epicsEnum16)

static long getUcharString(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    epicsUInt8 *psrc = (epicsUInt8 *) paddr->pfield;
    char *pdst = (char *) pto;

    if (nRequest==1 && offset==0) {
        cvtUcharToString(*psrc, pdst);
        return 0;
    }
    psrc += offset;
    while (nRequest--) {
        cvtUcharToString(*psrc, pdst);
        pdst += MAX_STRING_SIZE;
        if (++offset == no_elements)
            psrc = (epicsUInt8 *) paddr->pfield;
        else
            psrc++;
    }
    return 0;
}

static long getUcharChar GET_NOCONVERT(epicsUInt8, char)
static long getUcharUchar GET_NOCONVERT(epicsUInt8, epicsUInt8)
static long getUcharShort GET(epicsUInt8, epicsInt16)
static long getUcharUshort GET(epicsUInt8, epicsUInt16)
static long getUcharLong GET(epicsUInt8, epicsInt32)
static long getUcharUlong GET(epicsUInt8, epicsUInt32)
static long getUcharInt64 GET(epicsUInt8, epicsInt64)
static long getUcharUInt64 GET(epicsUInt8, epicsUInt64)
static long getUcharFloat GET(epicsUInt8, epicsFloat32)
static long getUcharDouble GET(epicsUInt8, epicsFloat64)
static long getUcharEnum GET(epicsUInt8, epicsEnum16)

static long getShortString(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    epicsInt16 *psrc = (epicsInt16 *) paddr->pfield;
    char *pdst = (char *) pto;

    if (nRequest==1 && offset==0) {
        cvtShortToString(*psrc, pdst);
        return 0;
    }
    psrc += offset;
    while (nRequest--) {
        cvtShortToString(*psrc, pdst);
        pdst += MAX_STRING_SIZE;
        if (++offset == no_elements)
            psrc = (epicsInt16 *) paddr->pfield;
        else
            psrc++;
    }
    return 0;
}

static long getShortChar GET(epicsInt16, char)
static long getShortUchar GET(epicsInt16, epicsUInt8)
static long getShortShort GET_NOCONVERT(epicsInt16, epicsInt16)
static long getShortUshort GET_NOCONVERT(epicsInt16, epicsUInt16)
static long getShortLong GET(epicsInt16, epicsInt32)
static long getShortUlong GET(epicsInt16, epicsUInt32)
static long getShortInt64 GET(epicsInt16, epicsInt64)
static long getShortUInt64 GET(epicsInt16, epicsUInt64)
static long getShortFloat GET(epicsInt16, epicsFloat32)
static long getShortDouble GET(epicsInt16, epicsFloat64)
static long getShortEnum GET(epicsInt16, epicsEnum16)

static long getUshortString(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    epicsUInt16 *psrc = (epicsUInt16 *) paddr->pfield;
    char *pdst = (char *) pto;

    if (nRequest==1 && offset==0) {
        cvtUshortToString(*psrc, pdst);
        return 0;
    }
    psrc += offset;
    while (nRequest--) {
        cvtUshortToString(*psrc, pdst);
        pdst += MAX_STRING_SIZE;
        if (++offset == no_elements)
            psrc = (epicsUInt16 *) paddr->pfield;
        else
            psrc++;
    }
    return 0;
}

static long getUshortChar GET(epicsUInt16, char)
static long getUshortUchar GET(epicsUInt16, epicsUInt8)
static long getUshortShort GET_NOCONVERT(epicsUInt16, epicsInt16)
static long getUshortUshort GET_NOCONVERT(epicsUInt16, epicsUInt16)
static long getUshortLong GET(epicsUInt16, epicsInt32)
static long getUshortUlong GET(epicsUInt16, epicsUInt32)
static long getUshortInt64 GET(epicsUInt16, epicsInt64)
static long getUshortUInt64 GET(epicsUInt16, epicsUInt64)
static long getUshortFloat GET(epicsUInt16, epicsFloat32)
static long getUshortDouble GET(epicsUInt16, epicsFloat64)
static long getUshortEnum GET(epicsUInt16, epicsEnum16)

static long getLongString(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    epicsInt32 *psrc = (epicsInt32 *) paddr->pfield;
    char *pdst = (char *) pto;

    if (nRequest==1 && offset==0) {
        cvtLongToString(*psrc, pdst);
        return 0;
    }
    psrc += offset;
    while (nRequest--) {
        cvtLongToString(*psrc, pdst);
        pdst += MAX_STRING_SIZE;
        if (++offset == no_elements)
            psrc = (epicsInt32 *) paddr->pfield;
        else
            psrc++;
    }
    return 0;
}

static long getLongChar GET(epicsInt32, char)
static long getLongUchar GET(epicsInt32, epicsUInt8)
static long getLongShort GET(epicsInt32, epicsInt16)
static long getLongUshort GET(epicsInt32, epicsUInt16)
static long getLongLong GET_NOCONVERT(epicsInt32, epicsInt32)
static long getLongUlong GET_NOCONVERT(epicsInt32, epicsUInt32)
static long getLongInt64 GET(epicsInt32, epicsInt64)
static long getLongUInt64 GET(epicsInt32, epicsUInt64)
static long getLongFloat GET(epicsInt32, epicsFloat32)
static long getLongDouble GET(epicsInt32, epicsFloat64)
static long getLongEnum GET(epicsInt32, epicsEnum16)

static long getUlongString(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    epicsUInt32 *psrc = (epicsUInt32 *) paddr->pfield;
    char *pdst = (char *) pto;

    if (nRequest==1 && offset==0) {
        cvtUlongToString(*psrc, pdst);
        return 0;
    }
    psrc += offset;
    while (nRequest--) {
        cvtUlongToString(*psrc, pdst);
        pdst += MAX_STRING_SIZE;
        if (++offset == no_elements)
            psrc = (epicsUInt32 *) paddr->pfield;
        else
            psrc++;
    }
    return 0;
}

static long getUlongChar GET(epicsUInt32, char)
static long getUlongUchar GET(epicsUInt32, epicsUInt8)
static long getUlongShort GET(epicsUInt32, epicsInt16)
static long getUlongUshort GET(epicsUInt32, epicsUInt16)
static long getUlongLong GET_NOCONVERT(epicsUInt32, epicsInt32)
static long getUlongUlong GET_NOCONVERT(epicsUInt32, epicsUInt32)
static long getUlongInt64 GET(epicsUInt32, epicsInt64)
static long getUlongUInt64 GET(epicsUInt32, epicsUInt64)
static long getUlongFloat GET(epicsUInt32, epicsFloat32)
static long getUlongDouble GET(epicsUInt32, epicsFloat64)
static long getUlongEnum GET(epicsUInt32, epicsEnum16)

static long getInt64String(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    epicsInt64 *psrc = (epicsInt64 *) paddr->pfield;
    char *pdst = (char *) pto;

    if (nRequest==1 && offset==0) {
        cvtInt64ToString(*psrc, pdst);
        return 0;
    }
    psrc += offset;
    while (nRequest--) {
        cvtInt64ToString(*psrc, pdst);
        pdst += MAX_STRING_SIZE;
        if (++offset == no_elements)
            psrc = (epicsInt64 *) paddr->pfield;
        else
            psrc++;
    }
    return 0;
}

static long getInt64Char GET(epicsInt64, char)
static long getInt64Uchar GET(epicsInt64, epicsUInt8)
static long getInt64Short GET(epicsInt64, epicsInt16)
static long getInt64Ushort GET(epicsInt64, epicsUInt16)
static long getInt64Long GET(epicsInt64, epicsInt32)
static long getInt64Ulong GET(epicsInt64, epicsUInt32)
static long getInt64Int64 GET_NOCONVERT(epicsInt64, epicsInt64)
static long getInt64UInt64 GET_NOCONVERT(epicsInt64, epicsUInt64)
static long getInt64Float GET(epicsInt64, epicsFloat32)
static long getInt64Double GET(epicsInt64, epicsFloat64)
static long getInt64Enum GET(epicsInt64, epicsEnum16)

static long getUInt64String(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    epicsUInt64 *psrc = (epicsUInt64 *) paddr->pfield;
    char *pdst = (char *) pto;

    if (nRequest==1 && offset==0) {
        cvtUInt64ToString(*psrc, pdst);
        return 0;
    }
    psrc += offset;
    while (nRequest--) {
        cvtUInt64ToString(*psrc, pdst);
        pdst += MAX_STRING_SIZE;
        if (++offset == no_elements)
            psrc = (epicsUInt64 *) paddr->pfield;
        else
            psrc++;
    }
    return 0;
}

static long getUInt64Char GET(epicsUInt64, char)
static long getUInt64Uchar GET(epicsUInt64, epicsUInt8)
static long getUInt64Short GET(epicsUInt64, epicsInt16)
static long getUInt64Ushort GET(epicsUInt64, epicsUInt16)
static long getUInt64Long GET(epicsUInt64, epicsInt32)
static long getUInt64Ulong GET(epicsUInt64, epicsUInt32)
static long getUInt64Int64 GET_NOCONVERT(epicsUInt64, epicsInt64)
static long getUInt64UInt64 GET_NOCONVERT(epicsUInt64, epicsUInt64)
static long getUInt64Float GET(epicsUInt64, epicsFloat32)
static long getUInt64Double GET(epicsUInt64, epicsFloat64)
static long getUInt64Enum GET(epicsUInt64, epicsEnum16)

static long getFloatString(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    epicsFloat32 *psrc = (epicsFloat32 *) paddr->pfield;
    char *pdst = (char *) pto;
    long status = 0;
    long precision = 6;
    rset *prset = 0;

    if (paddr)
        prset = dbGetRset(paddr);
    if (prset && prset->get_precision)
        status = prset->get_precision(paddr, &precision);
    if (nRequest==1 && offset==0) {
        cvtFloatToString(*psrc, pdst, precision);
        return(status);
    }
    psrc += offset;
    while (nRequest--) {
        cvtFloatToString(*psrc, pdst, precision);
        pdst += MAX_STRING_SIZE;
        if (++offset == no_elements)
            psrc = (epicsFloat32 *) paddr->pfield;
        else
            psrc++;
    }
    return(status);
}

static long getFloatChar GET(epicsFloat32, char)
static long getFloatUchar GET(epicsFloat32, epicsUInt8)
static long getFloatShort GET(epicsFloat32, epicsInt16)
static long getFloatUshort GET(epicsFloat32, epicsUInt16)
static long getFloatLong GET(epicsFloat32, epicsInt32)
static long getFloatUlong GET(epicsFloat32, epicsUInt32)
static long getFloatInt64 GET(epicsFloat32, epicsInt64)
static long getFloatUInt64 GET(epicsFloat32, epicsUInt64)
static long getFloatFloat GET_NOCONVERT(epicsFloat32, epicsFloat32)
static long getFloatDouble GET(epicsFloat32, epicsFloat64)
static long getFloatEnum GET(epicsFloat32, epicsEnum16)

static long getDoubleString(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    epicsFloat64 *psrc = (epicsFloat64 *) paddr->pfield;
    char *pdst = (char *) pto;
    long status = 0;
    long precision = 6;
    rset *prset = 0;

    if (paddr)
        prset = dbGetRset(paddr);
    if (prset && prset->get_precision)
        status = prset->get_precision(paddr, &precision);
    if (nRequest==1 && offset==0) {
        cvtDoubleToString(*psrc, pdst, precision);
        return(status);
    }
    psrc += offset;
    while (nRequest--) {
        cvtDoubleToString(*psrc, pdst, precision);
        pdst += MAX_STRING_SIZE;
        if (++offset == no_elements)
            psrc = (epicsFloat64 *) paddr->pfield;
        else
            psrc++;
    }
    return(status);
}

static long getDoubleChar GET(epicsFloat64, char)
static long getDoubleUchar GET(epicsFloat64, epicsUInt8)
static long getDoubleShort GET(epicsFloat64, epicsInt16)
static long getDoubleUshort GET(epicsFloat64, epicsUInt16)
static long getDoubleLong GET(epicsFloat64, epicsInt32)
static long getDoubleUlong GET(epicsFloat64, epicsUInt32)
static long getDoubleInt64 GET(epicsFloat64, epicsInt64)
static long getDoubleUInt64 GET(epicsFloat64, epicsUInt64)

static long getDoubleFloat(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    epicsFloat64 *psrc = (epicsFloat64 *) paddr->pfield;
    epicsFloat32 *pdst = (epicsFloat32 *) pto;

    if (nRequest==1 && offset==0) {
        *pdst = epicsConvertDoubleToFloat(*psrc);
        return 0;
    }
    psrc += offset;
    while (nRequest--) {
        *pdst = epicsConvertDoubleToFloat(*psrc);
        ++psrc; ++pdst;
        if (++offset == no_elements)
            psrc = (epicsFloat64 *) paddr->pfield;
    }
    return 0;
}

static long getDoubleDouble GET_NOCONVERT(epicsFloat64, epicsFloat64)
static long getDoubleEnum GET(epicsFloat64, epicsEnum16)

static long getEnumString(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    char *pdst = (char *) pto;
    rset *prset;
    long        status;

    prset = dbGetRset(paddr);
    if (prset && prset->get_enum_str)
        return prset->get_enum_str(paddr, pdst);

    status = S_db_noRSET;
    recGblRecSupError(status, paddr, "dbGet", "get_enum_str");
    return S_db_badDbrtype;
}

static long getEnumChar GET(epicsEnum16, char)
static long getEnumUchar GET(epicsEnum16, epicsUInt8)
static long getEnumShort GET(epicsEnum16, epicsInt16)
static long getEnumUshort GET(epicsEnum16, epicsUInt16)
static long getEnumLong GET(epicsEnum16, epicsInt32)
static long getEnumUlong GET(epicsEnum16, epicsUInt32)
static long getEnumInt64 GET(epicsEnum16, epicsInt64)
static long getEnumUInt64 GET(epicsEnum16, epicsUInt64)
static long getEnumFloat GET(epicsEnum16, epicsFloat32)
static long getEnumDouble GET(epicsEnum16, epicsFloat64)
static long getEnumEnum GET_NOCONVERT(epicsEnum16, epicsEnum16)

static long getMenuString(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    char                *pdst = (char *) pto;
    dbFldDes            *pdbFldDes = paddr->pfldDes;
    dbMenu              *pdbMenu;
    char                **papChoiceValue;
    char                *pchoice;
    epicsEnum16 choice_ind= *((epicsEnum16*) paddr->pfield);

    if (no_elements!=1){
        recGblDbaddrError(S_db_onlyOne, paddr, "dbGet(getMenuString)");
        return(S_db_onlyOne);
    }
    if (!pdbFldDes
    || !(pdbMenu = (dbMenu *) pdbFldDes->ftPvt)
    || (choice_ind>=pdbMenu->nChoice)
    || !(papChoiceValue = pdbMenu->papChoiceValue)
    || !(pchoice=papChoiceValue[choice_ind])) {
        recGblDbaddrError(S_db_badChoice, paddr, "dbGet(getMenuString)");
        return(S_db_badChoice);
    }
    strncpy(pdst, pchoice, MAX_STRING_SIZE);
    return 0;
}

static long getDeviceString(const dbAddr *paddr,
    void *pto, long nRequest, long no_elements, long offset)
{
    char                *pdst = (char *) pto;
    dbFldDes            *pdbFldDes = paddr->pfldDes;
    dbDeviceMenu        *pdbDeviceMenu;
    char                **papChoice;
    char                *pchoice;
    epicsEnum16 choice_ind= *((epicsEnum16*) paddr->pfield);

    if (no_elements!=1){
        recGblDbaddrError(S_db_onlyOne, paddr, "dbGet(getDeviceString)");
        return(S_db_onlyOne);
    }
    if (!pdbFldDes
    || !(pdbDeviceMenu = (dbDeviceMenu *) pdbFldDes->ftPvt)
    || (choice_ind>=pdbDeviceMenu->nChoice )
    || !(papChoice = pdbDeviceMenu->papChoice)
    || !(pchoice=papChoice[choice_ind])) {
        recGblDbaddrError(S_db_badChoice, paddr, "dbGet(getDeviceString)");
        return(S_db_badChoice);
    }
    strncpy(pdst, pchoice, MAX_STRING_SIZE);
    return 0;
}


/* dbAccess put conversion support routines */

static long putStringString(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const char *psrc = (const char *) pfrom;
    char *pdst = paddr->pfield;
    short size = paddr->field_size;

    if (nRequest==1 && offset==0) {
        strncpy(pdst, psrc, size);
        *(pdst+size-1) = 0;
        return 0;
    }
    pdst+= (size*offset);
    while (nRequest--) {
        strncpy(pdst, psrc, size);
        pdst[size-1] = 0;
        psrc += MAX_STRING_SIZE;
        if (++offset == no_elements)
            pdst = paddr->pfield;
        else
            pdst += size;
    }
    return 0;
}

static long putStringChar(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const char *psrc = pfrom;
    epicsInt8 *pdst = (epicsInt8 *) paddr->pfield + offset;

    while (nRequest--) {
        char *end;
        long status = epicsParseInt8(psrc, pdst++, 10, &end);

        if (status)
            return status;
        psrc += MAX_STRING_SIZE;
        if (++offset == no_elements)
            pdst = paddr->pfield;
    }
    return 0;
}

static long putStringUchar(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const char *psrc = pfrom;
    epicsUInt8 *pdst = (epicsUInt8 *) paddr->pfield + offset;

    while (nRequest--) {
        char *end;
        long status = epicsParseUInt8(psrc, pdst++, 10, &end);

        if (status)
            return status;
        psrc += MAX_STRING_SIZE;
        if (++offset == no_elements)
            pdst = paddr->pfield;
    }
    return 0;
}

static long putStringShort(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const char *psrc = pfrom;
    epicsInt16 *pdst = (epicsInt16 *) paddr->pfield + offset;

    while (nRequest--) {
        char *end;
        long status = epicsParseInt16(psrc, pdst++, 10, &end);

        if (status)
            return status;
        psrc += MAX_STRING_SIZE;
        if (++offset == no_elements)
            pdst = paddr->pfield;
    }
    return 0;
}

static long putStringUshort(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const char *psrc = pfrom;
    epicsUInt16 *pdst = (epicsUInt16 *) paddr->pfield + offset;

    while (nRequest--) {
        char *end;
        long status = epicsParseUInt16(psrc, pdst++, 10, &end);

        if (status)
            return status;
        psrc += MAX_STRING_SIZE;
        if (++offset == no_elements)
            pdst = paddr->pfield;
    }
    return 0;
}

static long putStringLong(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const char *psrc = pfrom;
    epicsInt32 *pdst = (epicsInt32 *) paddr->pfield + offset;

    while (nRequest--) {
        char *end;
        long status = epicsParseInt32(psrc, pdst++, 10, &end);

        if (status)
            return status;
        psrc += MAX_STRING_SIZE;
        if (++offset == no_elements)
            pdst = paddr->pfield;
    }
    return 0;
}

static long putStringUlong(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const char *psrc = pfrom;
    epicsUInt32 *pdst = (epicsUInt32 *) paddr->pfield + offset;

    while (nRequest--) {
        char *end;
        long status = epicsParseUInt32(psrc, pdst, 10, &end);

        if (status == S_stdlib_noConversion ||
            (!status && (*end == '.' || *end == 'e' || *end == 'E'))) {
            /*
             * Convert via double so numbers like 1.0e3 convert properly.
             * db_access pretends unsigned long is double.
             */
            epicsFloat64 dval;

            status = epicsParseFloat64(psrc, &dval, &end);
            if (!status && 0 <= dval && dval <= ULONG_MAX)
                *pdst = dval;
        }
        if (status)
            return status;
        psrc += MAX_STRING_SIZE;
        if (++offset == no_elements)
            pdst = paddr->pfield;
        else
            pdst++;
    }
    return 0;
}

static long putStringInt64(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const char *psrc = pfrom;
    epicsInt64 *pdst = (epicsInt64 *) paddr->pfield + offset;

    while (nRequest--) {
        char *end;
        long status = epicsParseInt64(psrc, pdst++, 10, &end);

        if (status)
            return status;

        psrc += MAX_STRING_SIZE;
        if (++offset == no_elements)
            pdst = paddr->pfield;
    }
    return 0;
}

static long putStringUInt64(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const char *psrc = pfrom;
    epicsUInt64 *pdst = (epicsUInt64 *) paddr->pfield + offset;

    while (nRequest--) {
        char *end;
        long status = epicsParseUInt64(psrc, pdst, 0, &end);

        if (status)
            return status;

        psrc += MAX_STRING_SIZE;
        if (++offset == no_elements)
            pdst = paddr->pfield;
        else
            pdst++;
    }
    return 0;
}

static long putStringFloat(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const char *psrc = pfrom;
    epicsFloat32 *pdst = (epicsFloat32 *) paddr->pfield + offset;

    while (nRequest--) {
        char *end;
        long status = epicsParseFloat32(psrc, pdst++, &end);

        if (status)
            return status;
        psrc += MAX_STRING_SIZE;
        if (++offset == no_elements)
            pdst = paddr->pfield;
    }
    return 0;
}

static long putStringDouble(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const char *psrc = pfrom;
    epicsFloat64 *pdst = (epicsFloat64 *) paddr->pfield + offset;

    while (nRequest--) {
        char *end;
        long status = epicsParseFloat64(psrc, pdst++, &end);

        if (status)
            return status;
        psrc += MAX_STRING_SIZE;
        if (++offset == no_elements)
            pdst = paddr->pfield;
    }
    return 0;
}

static long putStringEnum(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    epicsEnum16 *pfield = paddr->pfield;
    rset *prset = dbGetRset(paddr);
    long status = S_db_noRSET;
    struct dbr_enumStrs enumStrs;

    if (no_elements != 1) {
        recGblDbaddrError(S_db_onlyOne, paddr, "dbPut(putStringEnum)");
        return S_db_onlyOne;
    }

    if (!prset || !prset->put_enum_str) {
        recGblRecSupError(status, paddr, "dbPut(putStringEnum)", "put_enum_str");
        return status;
    }

    status = prset->put_enum_str(paddr, pfrom);
    if (!status)
        return status;

    if (!prset->get_enum_strs) {
        recGblRecSupError(status, paddr, "dbPut(putStringEnum)", "get_enum_strs");
        return status;
    }

    status = prset->get_enum_strs(paddr, &enumStrs);
    if (!status) {
        epicsEnum16 val;
        char *end;

        status = epicsParseUInt16(pfrom, &val, 10, &end);
        if (!status && val < enumStrs.no_str) {
            *pfield = val;
            return 0;
        }
        status = S_db_badChoice;
    }

    recGblRecordError(status, paddr->precord, pfrom);
    return status;
}

static long putStringMenu(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    dbFldDes *pdbFldDes = paddr->pfldDes;
    epicsEnum16 *pfield = paddr->pfield;
    dbMenu *pdbMenu;
    char **pchoices, *pchoice;

    if (no_elements != 1) {
        recGblDbaddrError(S_db_onlyOne, paddr, "dbPut(putStringMenu)");
        return S_db_onlyOne;
    }

    if (pdbFldDes &&
        (pdbMenu = pdbFldDes->ftPvt) &&
        (pchoices = pdbMenu->papChoiceValue)) {
        int i, nChoice = pdbMenu->nChoice;
        epicsEnum16 val;

        for (i = 0; i < nChoice; i++) {
            pchoice = pchoices[i];
            if (!pchoice)
                continue;
            if (strcmp(pchoice, pfrom) == 0) {
                *pfield = i;
                return 0;
            }
        }

        if (!epicsParseUInt16(pfrom, &val, 10, NULL)
            && val < nChoice) {
            *pfield = val;
            return 0;
        }
    }
    recGblDbaddrError(S_db_badChoice, paddr, "dbPut(putStringMenu)");
    return S_db_badChoice;
}

static long putStringDevice(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    dbFldDes *pdbFldDes = paddr->pfldDes;
    dbDeviceMenu *pdbDeviceMenu = pdbFldDes->ftPvt;
    epicsEnum16 *pfield = paddr->pfield;
    char **pchoices, *pchoice;

    if (no_elements != 1) {
        recGblDbaddrError(S_db_onlyOne, paddr, "dbPut(putStringDevice)");
        return S_db_onlyOne;
    }

    if (pdbFldDes &&
        (pdbDeviceMenu = pdbFldDes->ftPvt) &&
        (pchoices = pdbDeviceMenu->papChoice)) {
        int i, nChoice = pdbDeviceMenu->nChoice;
        epicsEnum16 val;

        for (i = 0; i < nChoice; i++) {
            pchoice = pchoices[i];
            if (!pchoice)
                continue;
            if (strcmp(pchoice, pfrom) == 0) {
                *pfield = i;
                return 0;
            }
        }

        if (!epicsParseUInt16(pfrom, &val, 10, NULL) && val < nChoice) {
            *pfield = val;
            return 0;
        }
    }
    recGblDbaddrError(S_db_badChoice, paddr, "dbPut(putStringDevice)");
    return S_db_badChoice;
}


static long putCharString(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const char *psrc = (const char *) pfrom;
    char *pdst = (char *) paddr->pfield;
    short size = paddr->field_size;

    if (nRequest==1 && offset==0) {
        cvtCharToString(*psrc, pdst);
        return 0;
    }
    pdst += (size*offset);
    while (nRequest--) {
        cvtCharToString(*psrc, pdst);
        psrc++;
        if (++offset == no_elements)
            pdst = paddr->pfield;
        else
            pdst += size;
    }
    return 0;
}

static long putCharChar PUT_NOCONVERT(char, char)
static long putCharUchar PUT_NOCONVERT(char, epicsUInt8)
static long putCharShort PUT(char, epicsInt16)
static long putCharUshort PUT(char, epicsUInt16)
static long putCharLong PUT(char, epicsInt32)
static long putCharUlong PUT(char, epicsUInt32)
static long putCharInt64 PUT(char, epicsInt64)
static long putCharUInt64 PUT(char, epicsUInt64)
static long putCharFloat PUT(char, epicsFloat32)
static long putCharDouble PUT(char, epicsFloat64)
static long putCharEnum PUT(char, epicsEnum16)

static long putUcharString(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const epicsUInt8 *psrc = (const epicsUInt8 *) pfrom;
    char *pdst = (char *) paddr->pfield;
    short size = paddr->field_size;


    if (nRequest==1 && offset==0) {
        cvtUcharToString(*psrc, pdst);
        return 0;
    }
    pdst += (size*offset);
    while (nRequest--) {
        cvtUcharToString(*psrc, pdst);
        psrc++;
        if (++offset == no_elements)
            pdst = paddr->pfield;
        else
            pdst += size;
    }
    return 0;
}

static long putUcharChar PUT_NOCONVERT(epicsUInt8, char)
static long putUcharUchar PUT_NOCONVERT(epicsUInt8, epicsUInt8)
static long putUcharShort PUT(epicsUInt8, epicsInt16)
static long putUcharUshort PUT(epicsUInt8, epicsUInt16)
static long putUcharLong PUT(epicsUInt8, epicsInt32)
static long putUcharUlong PUT(epicsUInt8, epicsUInt32)
static long putUcharInt64 PUT(epicsUInt8, epicsInt64)
static long putUcharUInt64 PUT(epicsUInt8, epicsUInt64)
static long putUcharFloat PUT(epicsUInt8, epicsFloat32)
static long putUcharDouble PUT(epicsUInt8, epicsFloat64)
static long putUcharEnum PUT(epicsUInt8, epicsEnum16)

static long putShortString(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const epicsInt16 *psrc = (const epicsInt16 *) pfrom;
    char *pdst = (char *) paddr->pfield;
    short size = paddr->field_size;


    if (nRequest==1 && offset==0) {
        cvtShortToString(*psrc, pdst);
        return 0;
    }
    pdst += (size*offset);
    while (nRequest--) {
        cvtShortToString(*psrc, pdst);
        psrc++;
        if (++offset == no_elements)
            pdst = (char *) paddr->pfield;
        else
            pdst += size;
    }
    return 0;
}

static long putShortChar PUT(epicsInt16, char)
static long putShortUchar PUT(epicsInt16, epicsUInt8)
static long putShortShort PUT_NOCONVERT(epicsInt16, epicsInt16)
static long putShortUshort PUT_NOCONVERT(epicsInt16, epicsUInt16)
static long putShortLong PUT(epicsInt16, epicsInt32)
static long putShortUlong PUT(epicsInt16, epicsUInt32)
static long putShortInt64 PUT(epicsInt16, epicsInt64)
static long putShortUInt64 PUT(epicsInt16, epicsUInt64)
static long putShortFloat PUT(epicsInt16, epicsFloat32)
static long putShortDouble PUT(epicsInt16, epicsFloat64)
static long putShortEnum PUT(epicsInt16, epicsEnum16)

static long putUshortString(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const epicsUInt16 *psrc = (const epicsUInt16 *) pfrom;
    char *pdst = (char *) paddr->pfield;
    short size = paddr->field_size;


    if (nRequest==1 && offset==0) {
        cvtUshortToString(*psrc, pdst);
        return 0;
    }
    pdst += (size*offset);
    while (nRequest--) {
        cvtUshortToString(*psrc, pdst);
        psrc++;
        if (++offset == no_elements)
            pdst = (char *) paddr->pfield;
        else
            pdst += size;
    }
    return 0;
}

static long putUshortChar PUT(epicsUInt16, char)
static long putUshortUchar PUT(epicsUInt16, epicsUInt8)
static long putUshortShort PUT_NOCONVERT(epicsUInt16, epicsInt16)
static long putUshortUshort PUT_NOCONVERT(epicsUInt16, epicsUInt16)
static long putUshortLong PUT(epicsUInt16, epicsInt32)
static long putUshortUlong PUT(epicsUInt16, epicsUInt32)
static long putUshortInt64 PUT(epicsUInt16, epicsInt64)
static long putUshortUInt64 PUT(epicsUInt16, epicsUInt64)
static long putUshortFloat PUT(epicsUInt16, epicsFloat32)
static long putUshortDouble PUT(epicsUInt16, epicsFloat64)
static long putUshortEnum PUT(epicsUInt16, epicsEnum16)

static long putLongString(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const epicsInt32 *psrc = (const epicsInt32 *) pfrom;
    char *pdst = (char *) paddr->pfield;
    short size = paddr->field_size;


    if (nRequest==1 && offset==0) {
        cvtLongToString(*psrc, pdst);
        return 0;
    }
    pdst += (size*offset);
    while (nRequest--) {
        cvtLongToString(*psrc, pdst);
        psrc++;
        if (++offset == no_elements)
            pdst = (char *) paddr->pfield;
        else
            pdst += size;
    }
    return 0;
}

static long putLongChar PUT(epicsInt32, char)
static long putLongUchar PUT(epicsInt32, epicsUInt8)
static long putLongShort PUT(epicsInt32, epicsInt16)
static long putLongUshort PUT(epicsInt32, epicsUInt16)
static long putLongLong PUT_NOCONVERT(epicsInt32, epicsInt32)
static long putLongUlong PUT_NOCONVERT(epicsInt32, epicsUInt32)
static long putLongInt64 PUT(epicsInt32, epicsInt64)
static long putLongUInt64 PUT(epicsInt32, epicsUInt64)
static long putLongFloat PUT(epicsInt32, epicsFloat32)
static long putLongDouble PUT(epicsInt32, epicsFloat64)
static long putLongEnum PUT(epicsInt32, epicsEnum16)

static long putUlongString(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const epicsUInt32 *psrc = (const epicsUInt32 *) pfrom;
    char *pdst = (char *) paddr->pfield;
    short size = paddr->field_size;


    if (nRequest==1 && offset==0) {
        cvtUlongToString(*psrc, pdst);
        return 0;
    }
    pdst += (size*offset);
    while (nRequest--) {
        cvtUlongToString(*psrc, pdst);
        psrc++;
        if (++offset == no_elements)
            pdst = (char *) paddr->pfield;
        else
            pdst += size;
    }
    return 0;
}

static long putUlongChar PUT(epicsUInt32, char)
static long putUlongUchar PUT(epicsUInt32, epicsUInt8)
static long putUlongShort PUT(epicsUInt32, epicsInt16)
static long putUlongUshort PUT(epicsUInt32, epicsUInt16)
static long putUlongLong PUT_NOCONVERT(epicsUInt32, epicsInt32)
static long putUlongUlong PUT_NOCONVERT(epicsUInt32, epicsUInt32)
static long putUlongInt64 PUT(epicsUInt32, epicsInt64)
static long putUlongUInt64 PUT(epicsUInt32, epicsUInt64)
static long putUlongFloat PUT(epicsUInt32, epicsFloat32)
static long putUlongDouble PUT(epicsUInt32, epicsFloat64)
static long putUlongEnum PUT(epicsUInt32, epicsEnum16)

static long putInt64String(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const epicsInt64 *psrc = (const epicsInt64 *) pfrom;
    char *pdst = (char *) paddr->pfield;
    short size=paddr->field_size;


    if (nRequest==1 && offset==0) {
        cvtInt64ToString(*psrc, pdst);
        return 0;
    }
    pdst += (size*offset);
    while (nRequest--) {
        cvtInt64ToString(*psrc, pdst);
        psrc++;
        if (++offset == no_elements)
            pdst = (char *) paddr->pfield;
        else
            pdst += size;
    }
    return 0;
}

static long putInt64Char PUT(epicsInt64, char)
static long putInt64Uchar PUT(epicsInt64, epicsUInt8)
static long putInt64Short PUT(epicsInt64, epicsInt16)
static long putInt64Ushort PUT(epicsInt64, epicsUInt16)
static long putInt64Long PUT(epicsInt64, epicsInt32)
static long putInt64Ulong PUT(epicsInt64, epicsUInt32)
static long putInt64Int64 PUT_NOCONVERT(epicsInt64, epicsInt64)
static long putInt64UInt64 PUT_NOCONVERT(epicsInt64, epicsUInt64)
static long putInt64Float PUT(epicsInt64, epicsFloat32)
static long putInt64Double PUT(epicsInt64, epicsFloat64)
static long putInt64Enum PUT(epicsInt64, epicsEnum16)

static long putUInt64String(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const epicsUInt64 *psrc = (const epicsUInt64 *) pfrom;
    char *pdst = (char *) paddr->pfield;
    short size=paddr->field_size;


    if (nRequest==1 && offset==0) {
        cvtUlongToString(*psrc, pdst);
        return 0;
    }
    pdst += (size*offset);
    while (nRequest--) {
        cvtUlongToString(*psrc, pdst);
        psrc++;
        if (++offset == no_elements)
            pdst = (char *) paddr->pfield;
        else
            pdst += size;
    }
    return 0;
}

static long putUInt64Char PUT(epicsUInt64, char)
static long putUInt64Uchar PUT(epicsUInt64, epicsUInt8)
static long putUInt64Short PUT(epicsUInt64, epicsInt16)
static long putUInt64Ushort PUT(epicsUInt64, epicsUInt16)
static long putUInt64Long PUT(epicsUInt64, epicsInt32)
static long putUInt64Ulong PUT(epicsUInt64, epicsUInt32)
static long putUInt64Int64 PUT_NOCONVERT(epicsUInt64, epicsInt64)
static long putUInt64UInt64 PUT_NOCONVERT(epicsUInt64, epicsUInt64)
static long putUInt64Float PUT(epicsUInt64, epicsFloat32)
static long putUInt64Double PUT(epicsUInt64, epicsFloat64)
static long putUInt64Enum PUT(epicsUInt64, epicsEnum16)

static long putFloatString(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const epicsFloat32 *psrc = (const epicsFloat32 *) pfrom;
    char *pdst = (char *) paddr->pfield;
    long status = 0;
    long precision = 6;
    rset *prset = 0;
    short size = paddr->field_size;

    if (paddr)
        prset = dbGetRset(paddr);
    if (prset && prset->get_precision)
        status = prset->get_precision(paddr, &precision);
    if (nRequest==1 && offset==0) {
        cvtFloatToString(*psrc, pdst, precision);
        return(status);
    }
    pdst += (size*offset);
    while (nRequest--) {
        cvtFloatToString(*psrc, pdst, precision);
        psrc++;
        if (++offset == no_elements)
            pdst = (char *) paddr->pfield;
        else
            pdst += size;
    }
    return(status);
}

static long putFloatChar PUT(epicsFloat32, char)
static long putFloatUchar PUT(epicsFloat32, epicsUInt8)
static long putFloatShort PUT(epicsFloat32, epicsInt16)
static long putFloatUshort PUT(epicsFloat32, epicsUInt16)
static long putFloatLong PUT(epicsFloat32, epicsInt32)
static long putFloatUlong PUT(epicsFloat32, epicsUInt32)
static long putFloatInt64 PUT(epicsFloat32, epicsInt64)
static long putFloatUInt64 PUT(epicsFloat32, epicsUInt64)
static long putFloatFloat PUT_NOCONVERT(epicsFloat32, epicsFloat32)
static long putFloatDouble PUT(epicsFloat32, epicsFloat64)
static long putFloatEnum PUT(epicsFloat32, epicsEnum16)

static long putDoubleString(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const epicsFloat64 *psrc = (const epicsFloat64 *) pfrom;
    char *pdst = (char *) paddr->pfield;
    long status = 0;
    long precision = 6;
    rset *prset = 0;
    short size = paddr->field_size;

    if (paddr)
        prset = dbGetRset(paddr);
    if (prset && prset->get_precision)
        status = prset->get_precision(paddr, &precision);
    if (nRequest==1 && offset==0) {
        cvtDoubleToString(*psrc, pdst, precision);
        return status;
    }
    pdst += (size*offset);
    while (nRequest--) {
        cvtDoubleToString(*psrc, pdst, precision);
        psrc++;
        if (++offset == no_elements)
            pdst = (char *) paddr->pfield;
        else
            pdst += size;
    }
    return status;
}

static long putDoubleChar PUT(epicsFloat64, char)
static long putDoubleUchar PUT(epicsFloat64, epicsUInt8)
static long putDoubleShort PUT(epicsFloat64, epicsInt16)
static long putDoubleUshort PUT(epicsFloat64, epicsUInt16)
static long putDoubleLong PUT(epicsFloat64, epicsInt32)
static long putDoubleUlong PUT(epicsFloat64, epicsUInt32)
static long putDoubleInt64 PUT(epicsFloat64, epicsInt64)
static long putDoubleUInt64 PUT(epicsFloat64, epicsUInt64)

static long putDoubleFloat(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const epicsFloat64 *psrc = (const epicsFloat64 *) pfrom;
    epicsFloat32 *pdst = (epicsFloat32 *) paddr->pfield;

    if (nRequest==1 && offset==0) {
        *pdst = epicsConvertDoubleToFloat(*psrc);
        return 0;
    }
    pdst += offset;
    while (nRequest--) {
        *pdst++ = epicsConvertDoubleToFloat(*psrc++);
        if (++offset == no_elements)
            pdst = (epicsFloat32 *) paddr->pfield;
    }
    return 0;
}

static long putDoubleDouble PUT_NOCONVERT(epicsFloat64, epicsFloat64)
static long putDoubleEnum PUT(epicsFloat64, epicsEnum16)

static long putEnumString(dbAddr *paddr,
    const void *pfrom, long nRequest, long no_elements, long offset)
{
    const epicsEnum16 *psrc = (const epicsEnum16 *) pfrom;
    char *pdst = (char *) paddr->pfield;
    short size = paddr->field_size;


    if (nRequest==1 && offset==0) {
        cvtUshortToString(*psrc, pdst);
        return 0;
    }
    pdst += (size*offset);
    while (nRequest--) {
        cvtUshortToString(*psrc, pdst);
        psrc++;
        if (++offset == no_elements)
            pdst = (char *) paddr->pfield;
        else
            pdst += size;
    }
    return 0;
}

static long putEnumChar PUT(epicsEnum16, char)
static long putEnumUchar PUT(epicsEnum16, epicsUInt8)
static long putEnumShort PUT(epicsEnum16, epicsInt16)
static long putEnumUshort PUT(epicsEnum16, epicsUInt16)
static long putEnumLong PUT(epicsEnum16, epicsInt32)
static long putEnumUlong PUT(epicsEnum16, epicsUInt32)
static long putEnumInt64 PUT(epicsEnum16, epicsInt64)
static long putEnumUInt64 PUT(epicsEnum16, epicsUInt64)
static long putEnumFloat PUT(epicsEnum16, epicsFloat32)
static long putEnumDouble PUT(epicsEnum16, epicsFloat64)
static long putEnumEnum PUT_NOCONVERT(epicsEnum16, epicsEnum16)

/* This is the table of routines for converting database fields */
/* the rows represent the field type of the database field */
/* the columns represent the types of the buffer in which they are placed */

/* buffer types are********************************************************
 DBR_STRING,      DBR_CHR,         DBR_UCHAR,       DBR_SHORT,       DBR_USHORT,
 DBR_LONG,        DBR_ULONG,       DBR_INT64,       DBR_UINT64,
 DBR_FLOAT,       DBR_DOUBLE,      DBR_ENUM
 ***************************************************************************/

epicsShareDef GETCONVERTFUNC dbGetConvertRoutine[DBF_DEVICE+1][DBR_ENUM+1] = {

/* source is a DBF_STRING               */
{getStringString, getStringChar,   getStringUchar,  getStringShort,  getStringUshort,
 getStringLong,   getStringUlong,  getStringInt64,  getStringUInt64,
 getStringFloat,  getStringDouble, getStringUshort},
/* source is a DBF_CHAR                 */
{getCharString,   getCharChar,     getCharUchar,    getCharShort,    getCharUshort,
 getCharLong,     getCharUlong,    getCharInt64,    getCharUInt64,
 getCharFloat,    getCharDouble,   getCharEnum},
/* source is a DBF_UCHAR                */
{getUcharString,  getUcharChar,    getUcharUchar,   getUcharShort,   getUcharUshort,
 getUcharLong,    getUcharUlong,   getUcharInt64,   getUcharUInt64,
 getUcharFloat,   getUcharDouble,  getUcharEnum},
/* source is a DBF_SHORT                */
{getShortString,  getShortChar,    getShortUchar,   getShortShort,   getShortUshort,
 getShortLong,    getShortUlong,   getShortInt64,   getShortUInt64,
 getShortFloat,   getShortDouble,  getShortEnum},
/* source is a DBF_USHORT               */
{getUshortString, getUshortChar,   getUshortUchar,  getUshortShort,  getUshortUshort,
 getUshortLong,   getUshortUlong,  getUshortInt64,  getUshortUInt64,
 getUshortFloat,  getUshortDouble, getUshortEnum},
/* source is a DBF_LONG                 */
{getLongString,   getLongChar,     getLongUchar,    getLongShort,    getLongUshort,
 getLongLong,     getLongUlong,    getLongInt64,    getLongUInt64,
 getLongFloat,    getLongDouble,   getLongEnum},
/* source is a DBF_ULONG                */
{getUlongString,  getUlongChar,    getUlongUchar,   getUlongShort,   getUlongUshort,
 getUlongLong,    getUlongUlong,   getUlongInt64,   getUlongUInt64,
 getUlongFloat,   getUlongDouble,  getUlongEnum},
/* source is a DBF_INT64                */
{getInt64String,  getInt64Char,    getInt64Uchar,   getInt64Short,   getInt64Ushort,
 getInt64Long,    getInt64Ulong,   getInt64Int64,   getInt64UInt64,
 getInt64Float,   getInt64Double,  getInt64Enum},
/* source is a DBF_UINT64               */
{getUInt64String, getUInt64Char,   getUInt64Uchar,  getUInt64Short,  getUInt64Ushort,
 getUInt64Long,   getUInt64Ulong,  getUInt64Int64,  getUInt64UInt64,
 getUInt64Float,  getUInt64Double, getUInt64Enum},
/* source is a DBF_FLOAT                */
{getFloatString,  getFloatChar,    getFloatUchar,   getFloatShort,   getFloatUshort,
 getFloatLong,    getFloatUlong,   getFloatInt64,   getFloatUInt64,
 getFloatFloat,   getFloatDouble,  getFloatEnum},
/* source is a DBF_DOUBLE               */
{getDoubleString, getDoubleChar,   getDoubleUchar,  getDoubleShort,  getDoubleUshort,
 getDoubleLong,   getDoubleUlong,  getDoubleInt64,  getDoubleUInt64,
 getDoubleFloat,  getDoubleDouble, getDoubleEnum},
/* source is a DBF_ENUM                 */
{getEnumString,   getEnumChar,     getEnumUchar,    getEnumShort,    getEnumUshort,
 getEnumLong,     getEnumUlong,    getEnumInt64,    getEnumUInt64,
 getEnumFloat,    getEnumDouble,   getEnumEnum},
/* source is a DBF_MENU                 */
{getMenuString,   getEnumChar,     getEnumUchar,    getEnumShort,    getEnumUshort,
 getEnumLong,     getEnumUlong,    getEnumInt64,    getEnumUInt64,
 getEnumFloat,    getEnumDouble,   getEnumEnum},
/* source is a DBF_DEVICE               */
{getDeviceString, getEnumChar,     getEnumUchar,    getEnumShort,    getEnumUshort,
 getEnumLong,     getEnumUlong,    getEnumInt64,    getEnumUInt64,
 getEnumFloat,    getEnumDouble,   getEnumEnum},
};

/* This is the table of routines for converting database fields */
/* the rows represent the buffer types                          */
/* the columns represent the field types                        */

/* field types are********************************************************
 DBF_STRING,      DBF_CHAR,        DBF_UCHAR,       DBF_SHORT,       DBF_USHORT,
 DBF_LONG,        DBF_ULONG,       DBF_INT64,       DBF_UINT64,
 DBF_FLOAT,       DBF_DOUBLE,      DBF_ENUM
 DBF_MENU,        DBF_DEVICE
 ***************************************************************************/

epicsShareDef PUTCONVERTFUNC dbPutConvertRoutine[DBR_ENUM+1][DBF_DEVICE+1] = {
/* source is a DBR_STRING               */
{putStringString, putStringChar,   putStringUchar,  putStringShort,  putStringUshort,
 putStringLong,   putStringUlong,  putStringInt64,  putStringUInt64,
 putStringFloat,  putStringDouble, putStringEnum,
 putStringMenu,   putStringDevice},
/* source is a DBR_CHAR                 */
{putCharString,   putCharChar,     putCharUchar,    putCharShort,    putCharUshort,
 putCharLong,     putCharUlong,    putCharInt64,    putCharUInt64,
 putCharFloat,    putCharDouble,   putCharEnum,
 putCharEnum,     putCharEnum},
/* source is a DBR_UCHAR                */
{putUcharString,  putUcharChar,    putUcharUchar,   putUcharShort,   putUcharUshort,
 putUcharLong,    putUcharUlong,   putUcharInt64,   putUcharUInt64,
 putUcharFloat,   putUcharDouble,  putUcharEnum,
 putUcharEnum,    putUcharEnum},
/* source is a DBR_SHORT                */
{putShortString,  putShortChar,    putShortUchar,   putShortShort,   putShortUshort,
 putShortLong,    putShortUlong,   putShortInt64,   putShortUInt64,
 putShortFloat,   putShortDouble,  putShortEnum,
 putShortEnum,    putShortEnum},
/* source is a DBR_USHORT               */
{putUshortString, putUshortChar,   putUshortUchar,  putUshortShort,  putUshortUshort,
 putUshortLong,   putUshortUlong,  putUshortInt64,  putUshortUInt64,
 putUshortFloat,  putUshortDouble, putUshortEnum,
 putUshortEnum,   putUshortEnum},
/* source is a DBR_LONG                 */
{putLongString,   putLongChar,     putLongUchar,    putLongShort,    putLongUshort,
 putLongLong,     putLongUlong,    putLongInt64,    putLongUInt64,
 putLongFloat,    putLongDouble,   putLongEnum,
 putLongEnum,     putLongEnum},
/* source is a DBR_ULONG                */
{putUlongString,  putUlongChar,    putUlongUchar,   putUlongShort,   putUlongUshort,
 putUlongLong,    putUlongUlong,   putUlongInt64,   putUlongUInt64,
 putUlongFloat,   putUlongDouble,  putUlongEnum,
 putUlongEnum,    putUlongEnum},
/* source is a DBR_INT64                */
{putInt64String,  putInt64Char,    putInt64Uchar,   putInt64Short,   putInt64Ushort,
 putInt64Long,    putInt64Ulong,   putInt64Int64,   putInt64UInt64,
 putInt64Float,   putInt64Double,  putInt64Enum,
 putInt64Enum,    putInt64Enum},
/* source is a DBR_UINT64               */
{putUInt64String, putUInt64Char,   putUInt64Uchar,  putUInt64Short,  putUInt64Ushort,
 putUInt64Long,   putUInt64Ulong,  putUInt64Int64,  putUInt64UInt64,
 putUInt64Float,  putUInt64Double, putUInt64Enum,
 putUInt64Enum,   putUInt64Enum},
/* source is a DBR_FLOAT                */
{putFloatString,  putFloatChar,    putFloatUchar,   putFloatShort,   putFloatUshort,
 putFloatLong,    putFloatUlong,   putFloatInt64,   putFloatUInt64,
 putFloatFloat,   putFloatDouble,  putFloatEnum,
 putFloatEnum,    putFloatEnum},
/* source is a DBR_DOUBLE               */
{putDoubleString, putDoubleChar,   putDoubleUchar,  putDoubleShort,  putDoubleUshort,
 putDoubleLong,   putDoubleUlong,  putDoubleInt64,  putDoubleUInt64,
 putDoubleFloat,  putDoubleDouble, putDoubleEnum,
 putDoubleEnum,   putDoubleEnum},
/* source is a DBR_ENUM                 */
{putEnumString,   putEnumChar,     putEnumUchar,    putEnumShort,    putEnumUshort,
 putEnumLong,     putEnumUlong,    putEnumInt64,    putEnumUInt64,
 putEnumFloat,    putEnumDouble,   putEnumEnum,
 putEnumEnum,     putEnumEnum}
};
