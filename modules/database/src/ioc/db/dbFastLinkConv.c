/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* dbFastLinkConv.c */
/*
 *      Author:            Matthew Needes
 *      Date:              12-9-93
 */

#include <stddef.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "alarm.h"
#include "cvtFast.h"
#include "dbDefs.h"
#include "epicsConvert.h"
#include "epicsStdlib.h"
#include "epicsStdio.h"
#include "errlog.h"
#include "errMdef.h"

#include "dbAccessDefs.h"
#include "dbAddr.h"
#include "dbBase.h"
#include "dbCommon.h"
#include "dbConvertFast.h"
#include "dbFldTypes.h"
#include "dbStaticLib.h"
#include "link.h"
#include "recGbl.h"
#include "recSup.h"
#include "special.h"


/*
 *  In the following functions:
 *
 *      cvt_y_z(ARGS)
 *
 *    converts from type y to type z.  If
 *     type y and z are the same, it merely copies.
 *
 *     where st - string
 *           c  - epicsInt8
 *           uc - epicsUInt8
 *           s  - epicsInt16
 *           us - epicsUInt16
 *           l  - epicsInt32
 *           ul - epicsUInt32
 *           q  - epicsInt64
 *           uq - epicsUInt64
 *           f  - epicsFloat32
 *           d  - epicsFloat64
 *           e  - enum
 *
 *  These functions are _single_ value functions,
 *       i.e.: do not deal with array types.
 */

/*
 *  A DB_LINK that is not initialized with recGblInitFastXXXLink()
 *     will have this conversion.
 */

/* Convert String to String */
static long cvt_st_st(const void *f, void *t, const dbAddr *paddr)
{
    const char *from = (const char *) f;
    char *to = (char *) t;
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

/* Convert String to Char */
static long cvt_st_c(const void *f, void *t, const dbAddr *paddr)
{
    const char *from = (const char *) f;
    epicsInt8 *to = (epicsInt8 *) t;
    char *end;

    if (*from == 0) {
        *to = 0;
        return 0;
    }
    return epicsParseInt8(from, to, 10, &end);
}

/* Convert String to Unsigned Char */
static long cvt_st_uc(const void *f, void *t, const dbAddr *paddr)
{
    const char *from = (const char *) f;
    epicsUInt8 *to = (epicsUInt8 *) t;
    char *end;

    if (*from == 0) {
        *to = 0;
        return 0;
    }
    return epicsParseUInt8(from, to, 10, &end);
}

/* Convert String to Short */
static long cvt_st_s(const void *f, void *t, const dbAddr *paddr)
{
    const char *from = (const char *) f;
    epicsInt16 *to = (epicsInt16 *) t;
    char *end;

    if (*from == 0) {
        *to = 0;
        return 0;
    }
    return epicsParseInt16(from, to, 10, &end);
}

/* Convert String to Unsigned Short */
static long cvt_st_us(const void *f, void *t, const dbAddr *paddr)
{
    const char *from = (const char *) f;
    epicsUInt16 *to = (epicsUInt16 *) t;
    char *end;

    if (*from == 0) {
        *to = 0;
        return 0;
    }
    return epicsParseUInt16(from, to, 10, &end);
}

/* Convert String to Long */
static long cvt_st_l(const void *f, void *t, const dbAddr *paddr)
{
    const char *from = (const char *) f;
    epicsInt32 *to = (epicsInt32 *) t;
    char *end;

    if (*from == 0) {
         *to = 0;
         return 0;
    }
    return epicsParseInt32(from, to, 10, &end);
}

/* Convert String to Unsigned Long */
static long cvt_st_ul(const void *f, void *t, const dbAddr *paddr)
{
    const char *from = (const char *) f;
    epicsUInt32 *to = (epicsUInt32 *) t;
    char *end;
    long status;

    if (*from == 0) {
       *to = 0;
       return 0;
    }
    status = epicsParseUInt32(from, to, 10, &end);
    if (status == S_stdlib_noConversion ||
        (!status && (*end == '.' || *end == 'e' || *end == 'E'))) {
        /*
         * Convert via double so numbers like 1.0e3 convert properly.
         * db_access pretends unsigned long is double.
         */
        double dval;

        status = epicsParseFloat64(from, &dval, &end);
        if (!status &&
            dval >=0 &&
            dval <= UINT_MAX)
            *to = dval;
    }
    return status;
}

/* Convert String to Int64 */
static long cvt_st_q(const void *f, void *t, const dbAddr *paddr)
{
    const char *from = (const char *) f;
    epicsInt64 *to = (epicsInt64 *) t;
    char *end;

    if (*from == 0) {
         *to = 0;
         return 0;
    }
    return epicsParseInt64(from, to, 10, &end);
}

/* Convert String to UInt64 */
static long cvt_st_uq(const void *f, void *t, const dbAddr *paddr)
{
    const char *from = (const char *) f;
    epicsUInt64 *to = (epicsUInt64 *) t;
    char *end;

    if (*from == 0) {
       *to = 0;
       return 0;
    }
    return epicsParseUInt64(from, to, 0, &end);
}

/* Convert String to Float */
static long cvt_st_f(const void *f, void *t, const dbAddr *paddr)
{
    const char *from = (const char *) f;
    epicsFloat32 *to = (epicsFloat32 *) t;
    char *end;

    if (*from == 0) {
        *to = 0;
        return 0;
    }
    return epicsParseFloat32(from, to, &end);
}

/* Convert String to Double */
static long cvt_st_d(const void *f, void *t, const dbAddr *paddr)
{
    const char *from = (const char *) f;
    epicsFloat64 *to = (epicsFloat64 *) t;
    char *end;

    if (*from == 0) {
        *to = 0.0;
        return 0;
    }
    return epicsParseFloat64(from, to, &end);
}

/* Convert String to Enumerated */
static long cvt_st_e(const void *f, void *t, const dbAddr *paddr)
{
    const char *from = (const char *) f;
    epicsEnum16 *to = (epicsEnum16 *) t;
    rset *prset = dbGetRset(paddr);
    long status = S_db_noRSET;
    struct dbr_enumStrs enumStrs;

    if (!prset || !prset->put_enum_str) {
        recGblRecSupError(status, paddr, "dbPutField", "put_enum_str");
        return status;
    }

    status = prset->put_enum_str(paddr, from);
    if (!status) return 0;

    if (!prset->get_enum_strs) {
        recGblRecSupError(status, paddr, "dbPutField", "get_enum_strs");
        return status;
    }

    status = prset->get_enum_strs(paddr, &enumStrs);
    if (!status) {
        epicsEnum16 val;

        status = epicsParseUInt16(from, &val, 10, NULL);
        if (!status && val < enumStrs.no_str) {
            *to = val;
            return 0;
        }
        status = S_db_badChoice;
    }

    recGblRecordError(status, paddr->precord, from);
    return status;
}

/* Convert String to Menu */
static long cvt_st_menu(const void *f, void *t, const dbAddr *paddr)
{
    const char *from = (const char *) f;
    epicsEnum16 *to = (epicsEnum16 *) t;
    dbFldDes *pdbFldDes = paddr->pfldDes;
    dbMenu *pdbMenu;
    char **pchoices;
    char *pchoice;

    if (pdbFldDes &&
        (pdbMenu = pdbFldDes->ftPvt) &&
        (pchoices = pdbMenu->papChoiceValue)) {
        int i, nChoice = pdbMenu->nChoice;
        epicsEnum16 val;

        for (i = 0; i < nChoice; i++) {
            pchoice = pchoices[i];
            if (!pchoice) continue;
            if (strcmp(pchoice, from) == 0) {
                *to = i;
                return 0;
            }
        }

        if (!epicsParseUInt16(from, &val, 10, NULL) && val < nChoice) {
            *to = val;
            return 0;
        }
    }
    recGblDbaddrError(S_db_badChoice, paddr, "dbFastLinkConv(cvt_st_menu)");
    return S_db_badChoice;
}

/* Convert String to Device */
static long cvt_st_device(const void *f, void *t, const dbAddr *paddr)
{
    const char *from = (const char *) f;
    epicsEnum16 *to = (epicsEnum16 *) t;
    dbFldDes *pdbFldDes = paddr->pfldDes;
    dbDeviceMenu *pdbDeviceMenu = pdbFldDes->ftPvt;
    char **pchoices, *pchoice;

    if (pdbFldDes &&
        (pdbDeviceMenu = pdbFldDes->ftPvt) &&
        (pchoices = pdbDeviceMenu->papChoice)) {
        int i, nChoice = pdbDeviceMenu->nChoice;
        epicsEnum16 val;

        for (i = 0; i < nChoice; i++) {
            pchoice = pchoices[i];
            if (!pchoice) continue;
            if (strcmp(pchoice, from) == 0) {
                *to = i;
                return 0;
            }
        }

        if (!epicsParseUInt16(from, &val, 10, NULL) && val < nChoice) {
            *to = val;
            return 0;
        }
    }
    recGblDbaddrError(S_db_badChoice, paddr, "dbFastLinkConv(cvt_st_device)");
    return S_db_badChoice;
}

/* Convert Char to String */
static long cvt_c_st(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt8 *from = (const epicsInt8 *) f;
    char *to = (char *) t;
    cvtCharToString(*from, to);
    return 0;
}

/* Convert Char to Char */
static long cvt_c_c(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt8 *from = (const epicsInt8 *) f;
    epicsInt8 *to = (epicsInt8 *) t;
    *to=*from;
    return 0;
}

/* Convert Char to Unsigned Char */
static long cvt_c_uc(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt8 *from = (const epicsInt8 *) f;
    epicsUInt8 *to = (epicsUInt8 *) t;
    *to=*from;
    return 0;
}

/* Convert Char to Short */
static long cvt_c_s(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt8 *from = (const epicsInt8 *) f;
    epicsInt16 *to = (epicsInt16 *) t;
    *to=*from;
    return 0;
}

/* Convert Char to Unsigned Short */
static long cvt_c_us(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt8 *from = (const epicsInt8 *) f;
    epicsUInt16 *to = (epicsUInt16 *) t;
    *to=*from;
    return 0;
}

/* Convert Char to Long */
static long cvt_c_l(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt8 *from = (const epicsInt8 *) f;
    epicsInt32 *to = (epicsInt32 *) t;
    *to=*from;
    return 0;
}

/* Convert Char to Unsigned Long */
static long cvt_c_ul(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt8 *from = (const epicsInt8 *) f;
    epicsUInt32 *to = (epicsUInt32 *) t;
    *to=*from;
    return 0;
}

/* Convert Char to Int64 */
static long cvt_c_q(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt8 *from = (const epicsInt8 *) f;
    epicsInt64 *to = (epicsInt64 *) t;
    *to=*from;
    return 0;
}

/* Convert Char to UInt64 */
static long cvt_c_uq(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt8 *from = (const epicsInt8 *) f;
    epicsUInt64 *to = (epicsUInt64 *) t;
    *to=*from;
    return 0;
}

/* Convert Char to Float */
static long cvt_c_f(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt8 *from = (const epicsInt8 *) f;
    epicsFloat32 *to = (epicsFloat32 *) t;
    *to=*from;
    return 0;
}

/* Convert Char to Double */
static long cvt_c_d(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt8 *from = (const epicsInt8 *) f;
    epicsFloat64 *to = (epicsFloat64 *) t;
    *to=*from;
    return 0;
}

/* Convert Char to Enumerated */
static long cvt_c_e(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt8 *from = (const epicsInt8 *) f;
    epicsEnum16 *to = (epicsEnum16 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Char to String */
static long cvt_uc_st(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt8 *from = (const epicsUInt8 *) f;
    char *to = (char *) t;
    cvtUcharToString(*from, to);
    return 0;
}

/* Convert Unsigned Char to Char */
static long cvt_uc_c(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt8 *from = (const epicsUInt8 *) f;
    epicsInt8 *to = (epicsInt8 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Char to Unsigned Char */
static long cvt_uc_uc(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt8 *from = (const epicsUInt8 *) f;
    epicsUInt8 *to = (epicsUInt8 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Char to Short */
static long cvt_uc_s(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt8 *from = (const epicsUInt8 *) f;
    epicsInt16 *to = (epicsInt16 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Char to Unsigned Short */
static long cvt_uc_us(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt8 *from = (const epicsUInt8 *) f;
    epicsUInt16 *to = (epicsUInt16 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Char to Long */
static long cvt_uc_l(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt8 *from = (const epicsUInt8 *) f;
    epicsInt32 *to = (epicsInt32 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Char to Unsigned Long */
static long cvt_uc_ul(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt8 *from = (const epicsUInt8 *) f;
    epicsUInt32 *to = (epicsUInt32 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Char to Int64 */
static long cvt_uc_q(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt8 *from = (const epicsUInt8 *) f;
    epicsInt64 *to = (epicsInt64 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Char to UInt64 */
static long cvt_uc_uq(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt8 *from = (const epicsUInt8 *) f;
    epicsUInt64 *to = (epicsUInt64 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Char to Float */
static long cvt_uc_f(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt8 *from = (const epicsUInt8 *) f;
    epicsFloat32 *to = (epicsFloat32 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Char to Double */
static long cvt_uc_d(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt8 *from = (const epicsUInt8 *) f;
    epicsFloat64 *to = (epicsFloat64 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Char to Enumerated */
static long cvt_uc_e(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt8 *from = (const epicsUInt8 *) f;
    epicsEnum16 *to = (epicsEnum16 *) t;
    *to=*from;
    return 0;
}

/* Convert Short to String */
static long cvt_s_st(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt16 *from = (const epicsInt16 *) f;
    char *to = (char *) t;
    cvtShortToString(*from, to);
    return 0;
}

/* Convert Short to Char */
static long cvt_s_c(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt16 *from = (const epicsInt16 *) f;
    epicsInt8 *to = (epicsInt8 *) t;
    *to=(epicsInt8)*from;
    return 0;
}

/* Convert Short to Unsigned Char */
static long cvt_s_uc(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt16 *from = (const epicsInt16 *) f;
    epicsUInt8 *to = (epicsUInt8 *) t;
    *to=(epicsUInt8)*from;
    return 0;
}

/* Convert Short to Short */
static long cvt_s_s(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt16 *from = (const epicsInt16 *) f;
    epicsInt16 *to = (epicsInt16 *) t;
    *to=*from;
    return 0;
}

/* Convert Short to Unsigned Short */
static long cvt_s_us(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt16 *from = (const epicsInt16 *) f;
    epicsUInt16 *to = (epicsUInt16 *) t;
    *to=*from;
    return 0;
}

/* Convert Short to Long */
static long cvt_s_l(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt16 *from = (const epicsInt16 *) f;
    epicsInt32 *to = (epicsInt32 *) t;
    *to=*from;
    return 0;
}

/* Convert Short to Unsigned Long */
static long cvt_s_ul(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt16 *from = (const epicsInt16 *) f;
    epicsUInt32 *to = (epicsUInt32 *) t;
    *to=*from;
    return 0;
}

/* Convert Short to Int64 */
static long cvt_s_q(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt16 *from = (const epicsInt16 *) f;
    epicsInt64 *to = (epicsInt64 *) t;
    *to=*from;
    return 0;
}

/* Convert Short to UInt64 */
static long cvt_s_uq(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt16 *from = (const epicsInt16 *) f;
    epicsUInt64 *to = (epicsUInt64 *) t;
    *to=*from;
    return 0;
}

/* Convert Short to Float */
static long cvt_s_f(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt16 *from = (const epicsInt16 *) f;
    epicsFloat32 *to = (epicsFloat32 *) t;
    *to=*from;
    return 0;
}

/* Convert Short to Double */
static long cvt_s_d(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt16 *from = (const epicsInt16 *) f;
    epicsFloat64 *to = (epicsFloat64 *) t;
    *to=*from;
    return 0;
}

/* Convert Short to Enumerated */
static long cvt_s_e(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt16 *from = (const epicsInt16 *) f;
    epicsEnum16 *to = (epicsEnum16 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Short to String */
static long cvt_us_st(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt16 *from = (const epicsUInt16 *) f;
    char *to = (char *) t;
    cvtUshortToString(*from, to);
    return 0;
}

/* Convert Unsigned Short to Char */
static long cvt_us_c(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt16 *from = (const epicsUInt16 *) f;
    epicsInt8 *to = (epicsInt8 *) t;
    *to=(epicsInt8)*from;
    return 0;
}

/* Convert Unsigned Short to Unsigned Char */
static long cvt_us_uc(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt16 *from = (const epicsUInt16 *) f;
    epicsUInt8 *to = (epicsUInt8 *) t;
    *to=(epicsUInt8)*from;
    return 0;
}

/* Convert Unsigned Short to Short */
static long cvt_us_s(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt16 *from = (const epicsUInt16 *) f;
    epicsInt16 *to = (epicsInt16 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Short to Unsigned Short */
static long cvt_us_us(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt16 *from = (const epicsUInt16 *) f;
    epicsUInt16 *to = (epicsUInt16 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Short to Long */
static long cvt_us_l(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt16 *from = (const epicsUInt16 *) f;
    epicsInt32 *to = (epicsInt32 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Short to Unsigned Long */
static long cvt_us_ul(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt16 *from = (const epicsUInt16 *) f;
    epicsUInt32 *to = (epicsUInt32 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Short to Int64 */
static long cvt_us_q(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt16 *from = (const epicsUInt16 *) f;
    epicsInt64 *to = (epicsInt64 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Short to UInt64 */
static long cvt_us_uq(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt16 *from = (const epicsUInt16 *) f;
    epicsUInt64 *to = (epicsUInt64 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Short to Float */
static long cvt_us_f(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt16 *from = (const epicsUInt16 *) f;
    epicsFloat32 *to = (epicsFloat32 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Short to Double */
static long cvt_us_d(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt16 *from = (const epicsUInt16 *) f;
    epicsFloat64 *to = (epicsFloat64 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Short to Enumerated */
static long cvt_us_e(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt16 *from = (const epicsUInt16 *) f;
    epicsUInt16 *to = (epicsUInt16 *) t;
    *to=*from;
    return 0;
}

/* Convert Long to String */
static long cvt_l_st(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt32 *from = (const epicsInt32 *) f;
    char *to = (char *) t;
    cvtLongToString(*from, to);
    return 0;
}

/* Convert Long to Char */
static long cvt_l_c(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt32 *from = (const epicsInt32 *) f;
    epicsInt8 *to = (epicsInt8 *) t;
    *to=*from;
    return 0;
}

/* Convert Long to Unsigned Char */
static long cvt_l_uc(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt32 *from = (const epicsInt32 *) f;
    epicsUInt8 *to = (epicsUInt8 *) t;
    *to=*from;
    return 0;
}

/* Convert Long to Short */
static long cvt_l_s(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt32 *from = (const epicsInt32 *) f;
    epicsInt16 *to = (epicsInt16 *) t;
    *to=*from;
    return 0;
}

/* Convert Long to Unsigned Short */
static long cvt_l_us(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt32 *from = (const epicsInt32 *) f;
    epicsUInt16 *to = (epicsUInt16 *) t;
    *to=*from;
    return 0;
}

/* Convert Long to Long */
static long cvt_l_l(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt32 *from = (const epicsInt32 *) f;
    epicsInt32 *to = (epicsInt32 *) t;
    *to=*from;
    return 0;
}

/* Convert Long to Unsigned Long */
static long cvt_l_ul(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt32 *from = (const epicsInt32 *) f;
    epicsUInt32 *to = (epicsUInt32 *) t;
    *to=*from;
    return 0;
}

/* Convert Long to Int64 */
static long cvt_l_q(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt32 *from = (const epicsInt32 *) f;
    epicsInt64 *to = (epicsInt64 *) t;
    *to=*from;
    return 0;
}

/* Convert Long to UInt64 */
static long cvt_l_uq(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt32 *from = (const epicsInt32 *) f;
    epicsUInt64 *to = (epicsUInt64 *) t;
    *to=*from;
    return 0;
}

/* Convert Long to Float */
static long cvt_l_f(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt32 *from = (const epicsInt32 *) f;
    epicsFloat32 *to = (epicsFloat32 *) t;
    *to=(epicsFloat32)*from;
    return 0;
}

/* Convert Long to Double */
static long cvt_l_d(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt32 *from = (const epicsInt32 *) f;
    epicsFloat64 *to = (epicsFloat64 *) t;
    *to=*from;
    return 0;
}

/* Convert Long to Enumerated */
static long cvt_l_e(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt32 *from = (const epicsInt32 *) f;
    epicsEnum16 *to = (epicsEnum16 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Long to String */
static long cvt_ul_st(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt32 *from = (const epicsUInt32 *) f;
    char *to = (char *) t;
    cvtUlongToString(*from, to);
    return 0;
}

/* Convert Unsigned Long to Char */
static long cvt_ul_c(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt32 *from = (const epicsUInt32 *) f;
    epicsInt8 *to = (epicsInt8 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Long to Unsigned Char */
static long cvt_ul_uc(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt32 *from = (const epicsUInt32 *) f;
    epicsUInt8 *to = (epicsUInt8 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Long to Short */
static long cvt_ul_s(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt32 *from = (const epicsUInt32 *) f;
    epicsInt16 *to = (epicsInt16 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Long to Unsigned Short */
static long cvt_ul_us(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt32 *from = (const epicsUInt32 *) f;
    epicsUInt16 *to = (epicsUInt16 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Long to Long */
static long cvt_ul_l(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt32 *from = (const epicsUInt32 *) f;
    epicsInt32 *to = (epicsInt32 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Long to Unsigned Long */
static long cvt_ul_ul(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt32 *from = (const epicsUInt32 *) f;
    epicsUInt32 *to = (epicsUInt32 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Long to Int64 */
static long cvt_ul_q(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt32 *from = (const epicsUInt32 *) f;
    epicsInt64 *to = (epicsInt64 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Long to UInt64 */
static long cvt_ul_uq(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt32 *from = (const epicsUInt32 *) f;
    epicsUInt64 *to = (epicsUInt64 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Long to Float */
static long cvt_ul_f(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt32 *from = (const epicsUInt32 *) f;
    epicsFloat32 *to = (epicsFloat32 *) t;
    *to=(epicsFloat32)*from;
    return 0;
}

/* Convert Unsigned Long to Double */
static long cvt_ul_d(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt32 *from = (const epicsUInt32 *) f;
    epicsFloat64 *to = (epicsFloat64 *) t;
    *to=*from;
    return 0;
}

/* Convert Unsigned Long to Enumerated */
static long cvt_ul_e(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt32 *from = (const epicsUInt32 *) f;
    epicsEnum16 *to = (epicsEnum16 *) t;
    *to=*from;
    return 0;
}

/* Convert Int64 to String */
static long cvt_q_st(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt64 *from = (const epicsInt64 *) f;
    char *to = (char *) t;
    cvtInt64ToString(*from, to);
    return 0;
}

/* Convert Int64 to Char */
static long cvt_q_c(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt64 *from = (const epicsInt64 *) f;
    epicsInt8 *to = (epicsInt8 *) t;
    *to=*from;
    return 0;
}

/* Convert Int64 to Unsigned Char */
static long cvt_q_uc(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt64 *from = (const epicsInt64 *) f;
    epicsUInt8 *to = (epicsUInt8 *) t;
    *to=*from;
    return 0;
}

/* Convert Int64 to Short */
static long cvt_q_s(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt64 *from = (const epicsInt64 *) f;
    epicsInt16 *to = (epicsInt16 *) t;
    *to=*from;
    return 0;
}

/* Convert Int64 to Unsigned Short */
static long cvt_q_us(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt64 *from = (const epicsInt64 *) f;
    epicsUInt16 *to = (epicsUInt16 *) t;
    *to=*from;
    return 0;
}

/* Convert Int64 to Long */
static long cvt_q_l(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt64 *from = (const epicsInt64 *) f;
    epicsInt32 *to = (epicsInt32 *) t;
    *to=*from;
    return 0;
}

/* Convert Int64 to Unsigned Long */
static long cvt_q_ul(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt64 *from = (const epicsInt64 *) f;
    epicsUInt32 *to = (epicsUInt32 *) t;
    *to=*from;
    return 0;
}

/* Convert Int64 to Int64 */
static long cvt_q_q(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt64 *from = (const epicsInt64 *) f;
    epicsInt64 *to = (epicsInt64 *) t;
    *to=*from;
    return 0;
}

/* Convert Int64 to UInt64 */
static long cvt_q_uq(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt64 *from = (const epicsInt64 *) f;
    epicsUInt64 *to = (epicsUInt64 *) t;
    *to=*from;
    return 0;
}

/* Convert Int64 to Float */
static long cvt_q_f(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt64 *from = (const epicsInt64 *) f;
    epicsFloat32 *to = (epicsFloat32 *) t;
    *to=*from;
    return 0;
}

/* Convert Int64 to Double */
static long cvt_q_d(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt64 *from = (const epicsInt64 *) f;
    epicsFloat64 *to = (epicsFloat64 *) t;
    *to=*from;
    return 0;
}

/* Convert Int64 to Enumerated */
static long cvt_q_e(const void *f, void *t, const dbAddr *paddr)
{
    const epicsInt64 *from = (const epicsInt64 *) f;
    epicsEnum16 *to = (epicsEnum16 *) t;
    *to=*from;
    return 0;
}

/* Convert UInt64 to String */
static long cvt_uq_st(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt64 *from = (const epicsUInt64 *) f;
    char *to = (char *) t;
    cvtUInt64ToString(*from, to);
    return 0;
}

/* Convert UInt64 to Char */
static long cvt_uq_c(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt64 *from = (const epicsUInt64 *) f;
    epicsInt8 *to = (epicsInt8 *) t;
    *to=*from;
    return 0;
}

/* Convert UInt64 to Unsigned Char */
static long cvt_uq_uc(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt64 *from = (const epicsUInt64 *) f;
    epicsUInt8 *to = (epicsUInt8 *) t;
    *to=*from;
    return 0;
}

/* Convert UInt64 to Short */
static long cvt_uq_s(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt64 *from = (const epicsUInt64 *) f;
    epicsInt16 *to = (epicsInt16 *) t;
    *to=*from;
    return 0;
}

/* Convert UInt64 to Unsigned Short */
static long cvt_uq_us(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt64 *from = (const epicsUInt64 *) f;
    epicsUInt16 *to = (epicsUInt16 *) t;
    *to=*from;
    return 0;
}

/* Convert UInt64 to Long */
static long cvt_uq_l(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt64 *from = (const epicsUInt64 *) f;
    epicsInt32 *to = (epicsInt32 *) t;
    *to=*from;
    return 0;
}

/* Convert UInt64 to Unsigned Long */
static long cvt_uq_ul(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt64 *from = (const epicsUInt64 *) f;
    epicsUInt32 *to = (epicsUInt32 *) t;
    *to=*from;
    return 0;
}

/* Convert UInt64 to Int64 */
static long cvt_uq_q(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt64 *from = (const epicsUInt64 *) f;
    epicsInt64 *to = (epicsInt64 *) t;
    *to=*from;
    return 0;
}

/* Convert UInt64 to UInt64 */
static long cvt_uq_uq(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt64 *from = (const epicsUInt64 *) f;
    epicsUInt64 *to = (epicsUInt64 *) t;
    *to=*from;
    return 0;
}

/* Convert UInt64 to Float */
static long cvt_uq_f(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt64 *from = (const epicsUInt64 *) f;
    epicsFloat32 *to = (epicsFloat32 *) t;
    *to=*from;
    return 0;
}

/* Convert UInt64 to Double */
static long cvt_uq_d(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt64 *from = (const epicsUInt64 *) f;
    epicsFloat64 *to = (epicsFloat64 *) t;
    *to=*from;
    return 0;
}

/* Convert UInt64 to Enumerated */
static long cvt_uq_e(const void *f, void *t, const dbAddr *paddr)
{
    const epicsUInt64 *from = (const epicsUInt64 *) f;
    epicsEnum16 *to = (epicsEnum16 *) t;
    *to=*from;
    return 0;
}

/* Convert Float to String */
static long cvt_f_st(const void *f, void *t, const dbAddr *paddr)
{
    const epicsFloat32 *from = (const epicsFloat32 *) f;
    char *to = (char *) t;
    
    rset *prset = 0;
    long status = 0;
    long precision = 6;

    if (paddr) prset = dbGetRset(paddr);

    if (prset && prset->get_precision)
        status = (*prset->get_precision)(paddr, &precision);
    cvtFloatToString(*from, to, (unsigned short)precision);
    return status;
 }

/* Convert Float to Char */
static long cvt_f_c(const void *f, void *t, const dbAddr *paddr)
{
    const epicsFloat32 *from = (const epicsFloat32 *) f;
    epicsInt8 *to = (epicsInt8 *) t;
    *to=(epicsInt8)*from;
    return 0;
}

/* Convert Float to Unsigned Char */
static long cvt_f_uc(const void *f, void *t, const dbAddr *paddr)
{
    const epicsFloat32 *from = (const epicsFloat32 *) f;
    epicsUInt8 *to = (epicsUInt8 *) t;
    *to=(epicsUInt8)*from;
    return 0;
}

/* Convert Float to Short */
static long cvt_f_s(const void *f, void *t, const dbAddr *paddr)
{
    const epicsFloat32 *from = (const epicsFloat32 *) f;
    epicsInt16 *to = (epicsInt16 *) t;
    *to=(epicsInt16)*from;
    return 0;
}

/* Convert Float to Unsigned Short */
static long cvt_f_us(const void *f, void *t, const dbAddr *paddr)
{
    const epicsFloat32 *from = (const epicsFloat32 *) f;
    epicsUInt16 *to = (epicsUInt16 *) t;
    *to=(epicsUInt16)*from;
    return 0;
}

/* Convert Float to Long */
static long cvt_f_l(const void *f, void *t, const dbAddr *paddr)
{
    const epicsFloat32 *from = (const epicsFloat32 *) f;
    epicsInt32 *to = (epicsInt32 *) t;
    *to=(epicsInt32)*from;
    return 0;
}

/* Convert Float to Unsigned Long */
static long cvt_f_ul(const void *f, void *t, const dbAddr *paddr)
{
    const epicsFloat32 *from = (const epicsFloat32 *) f;
    epicsUInt32 *to = (epicsUInt32 *) t;
    *to=(epicsUInt32)*from;
    return 0;
}

/* Convert Float to Int64 */
static long cvt_f_q(const void *f, void *t, const dbAddr *paddr)
{
    const epicsFloat32 *from = (const epicsFloat32 *) f;
    epicsInt64 *to = (epicsInt64 *) t;
    *to=*from;
    return 0;
}

/* Convert Float to UInt64 */
static long cvt_f_uq(const void *f, void *t, const dbAddr *paddr)
{
    const epicsFloat32 *from = (const epicsFloat32 *) f;
    epicsUInt64 *to = (epicsUInt64 *) t;
    *to=*from;
    return 0;
}

/* Convert Float to Float */
static long cvt_f_f(const void *f, void *t, const dbAddr *paddr)
{
    const epicsFloat32 *from = (const epicsFloat32 *) f;
    epicsFloat32 *to = (epicsFloat32 *) t;
    *to=*from;
    return 0;
}

/* Convert Float to Double */
static long cvt_f_d(const void *f, void *t, const dbAddr *paddr)
{
    const epicsFloat32 *from = (const epicsFloat32 *) f;
    epicsFloat64 *to = (epicsFloat64 *) t;
    *to=*from;
    return 0;
}

/* Convert Float to Enumerated */
static long cvt_f_e(const void *f, void *t, const dbAddr *paddr)
{
    const epicsFloat32 *from = (const epicsFloat32 *) f;
    epicsEnum16 *to = (epicsEnum16 *) t;
    *to=(epicsEnum16)*from;
    return 0;
}

/* Convert Double to String */
static long cvt_d_st(const void *f, void *t, const dbAddr *paddr)
{
    const epicsFloat64 *from = (const epicsFloat64 *) f;
    char *to = (char *) t;
    rset *prset = 0;
    long status = 0;
    long precision = 6;

    if (paddr) prset = dbGetRset(paddr);

    if (prset && prset->get_precision)
        status = (*prset->get_precision)(paddr, &precision);
    cvtDoubleToString(*from, to, (unsigned short)precision);
    return status;
 }

/* Convert Double to Char */
static long cvt_d_c(const void *f, void *t, const dbAddr *paddr)
{
    const epicsFloat64 *from = (const epicsFloat64 *) f;
    epicsInt8 *to = (epicsInt8 *) t;
    *to=(epicsInt8)*from;
    return 0;
}

/* Convert Double to Unsigned Char */
static long cvt_d_uc(const void *f, void *t, const dbAddr *paddr)
{
    const epicsFloat64 *from = (const epicsFloat64 *) f;
    epicsUInt8 *to = (epicsUInt8 *) t;
    *to=(epicsUInt8)*from;
    return 0;
}

/* Convert Double to Short */
static long cvt_d_s(const void *f, void *t, const dbAddr *paddr)
{
    const epicsFloat64 *from = (const epicsFloat64 *) f;
    epicsInt16 *to = (epicsInt16 *) t;
    *to=(epicsInt16)*from;
    return 0;
}

/* Convert Double to Unsigned Short */
static long cvt_d_us(const void *f, void *t, const dbAddr *paddr)
{
    const epicsFloat64 *from = (const epicsFloat64 *) f;
    epicsUInt16 *to = (epicsUInt16 *) t;
    *to=(epicsUInt16)*from;
    return 0;
}

/* Convert Double to Long */
static long cvt_d_l(const void *f, void *t, const dbAddr *paddr)
{
    const epicsFloat64 *from = (const epicsFloat64 *) f;
    epicsInt32 *to = (epicsInt32 *) t;
    *to=*from;
    return 0;
}

/* Convert Double to Unsigned Long */
static long cvt_d_ul(const void *f, void *t, const dbAddr *paddr)
{
    const epicsFloat64 *from = (const epicsFloat64 *) f;
    epicsUInt32 *to = (epicsUInt32 *) t;
    *to=*from;
    return 0;
}

/* Convert Double to Int64 */
static long cvt_d_q(const void *f, void *t, const dbAddr *paddr)
{
    const epicsFloat64 *from = (const epicsFloat64 *) f;
    epicsInt64 *to = (epicsInt64 *) t;
    *to=*from;
    return 0;
}

/* Convert Double to UInt64 */
static long cvt_d_uq(const void *f, void *t, const dbAddr *paddr)
{
    const epicsFloat64 *from = (const epicsFloat64 *) f;
    epicsUInt64 *to = (epicsUInt64 *) t;
    *to=*from;
    return 0;
}

/* Convert Double to Float */
static long cvt_d_f(const void *f, void *t, const dbAddr *paddr)
{
    const epicsFloat64 *from = (const epicsFloat64 *) f;
    epicsFloat32 *to = (epicsFloat32 *) t;
    *to = epicsConvertDoubleToFloat(*from); return 0;}

/* Convert Double to Double */
static long cvt_d_d(const void *f, void *t, const dbAddr *paddr)
{
    const epicsFloat64 *from = (const epicsFloat64 *) f;
    epicsFloat64 *to = (epicsFloat64 *) t;
    *to=*from;
    return 0;
}

/* Convert Double to Enumerated */
static long cvt_d_e(const void *f, void *t, const dbAddr *paddr)
{
    const epicsFloat64 *from = (const epicsFloat64 *) f;
    epicsEnum16 *to = (epicsEnum16 *) t;
    *to=(epicsEnum16)*from;
    return 0;
}

/* Convert Enumerated to Char */
static long cvt_e_c(const void *f, void *t, const dbAddr *paddr)
{
    const epicsEnum16 *from = (const epicsEnum16 *) f;
    epicsInt8 *to = (epicsInt8 *) t;
    *to=(epicsInt8)*from;
    return 0;
}

/* Convert Enumerated to Unsigned Char */
static long cvt_e_uc(const void *f, void *t, const dbAddr *paddr)
{
    const epicsEnum16 *from = (const epicsEnum16 *) f;
    epicsUInt8 *to = (epicsUInt8 *) t;
    *to=(epicsUInt8)*from;
    return 0;
}

/* Convert Enumerated to Short */
static long cvt_e_s(const void *f, void *t, const dbAddr *paddr)
{
    const epicsEnum16 *from = (const epicsEnum16 *) f;
    epicsInt16 *to = (epicsInt16 *) t;
    *to=*from;
    return 0;
}

/* Convert Enumerated to Unsigned Short */
static long cvt_e_us(const void *f, void *t, const dbAddr *paddr)
{
    const epicsEnum16 *from = (const epicsEnum16 *) f;
    epicsUInt16 *to = (epicsUInt16 *) t;
    *to=*from;
    return 0;
}

/* Convert Enumerated to Long */
static long cvt_e_l(const void *f, void *t, const dbAddr *paddr)
{
    const epicsEnum16 *from = (const epicsEnum16 *) f;
    epicsInt32 *to = (epicsInt32 *) t;
    *to=*from;
    return 0;
}

/* Convert Enumerated to Unsigned Long */
static long cvt_e_ul(const void *f, void *t, const dbAddr *paddr)
{
    const epicsEnum16 *from = (const epicsEnum16 *) f;
    epicsUInt32 *to = (epicsUInt32 *) t;
    *to=*from;
    return 0;
}

/* Convert Enumerated to Int64 */
static long cvt_e_q(const void *f, void *t, const dbAddr *paddr)
{
    const epicsEnum16 *from = (const epicsEnum16 *) f;
    epicsInt64 *to = (epicsInt64 *) t;
    *to=*from;
    return 0;
}

/* Convert Enumerated to UInt64 */
static long cvt_e_uq(const void *f, void *t, const dbAddr *paddr)
{
    const epicsEnum16 *from = (const epicsEnum16 *) f;
    epicsUInt64 *to = (epicsUInt64 *) t;
    *to=*from;
    return 0;
}

/* Convert Enumerated to Float */
static long cvt_e_f(const void *f, void *t, const dbAddr *paddr)
{
    const epicsEnum16 *from = (const epicsEnum16 *) f;
    epicsFloat32 *to = (epicsFloat32 *) t;
    *to=*from;
    return 0;
}

/* Convert Enumerated to Double */
static long cvt_e_d(const void *f, void *t, const dbAddr *paddr)
{
    const epicsEnum16 *from = (const epicsEnum16 *) f;
    epicsFloat64 *to = (epicsFloat64 *) t;
    *to=*from;
    return 0;
}

/* Convert Enumerated to Enumerated */
static long cvt_e_e(const void *f, void *t, const dbAddr *paddr)
{
    const epicsEnum16 *from = (const epicsEnum16 *) f;
    epicsEnum16 *to = (epicsEnum16 *) t;
    *to=*from;
    return 0;
}

/* Convert Choices And Enumerated Types To String ... */

/* Get Enumerated to String */
static long cvt_e_st_get(const void *f, void *t, const dbAddr *paddr)
{
    char *to = (char *) t;
    rset *prset = 0;
    long status;

    if(paddr) prset = dbGetRset(paddr);

    if (prset && prset->get_enum_str)
        return (*prset->get_enum_str)(paddr, to);

    status = S_db_noRSET;
    recGblRecSupError(status, paddr, "dbGetField", "get_enum_str");

    return S_db_badDbrtype;
}

/* Put Enumerated to String */
static long cvt_e_st_put(const void *f, void *t, const dbAddr *paddr)
{
    const epicsEnum16 *from = (const epicsEnum16 *) f;
    char *to = (char *) t;

    cvtUshortToString(*from, to);
    return 0;
}

/* Get Menu to String */
static long cvt_menu_st(const void *f, void *t, const dbAddr *paddr)
{
    const epicsEnum16 *from = (const epicsEnum16 *) f;
    char *to = (char *) t;
    dbFldDes *pdbFldDes;
    dbMenu *pdbMenu;

    if (!paddr ||
        !(pdbFldDes = paddr->pfldDes) ||
        !(pdbMenu = (dbMenu *)pdbFldDes->ftPvt)) {
        recGblDbaddrError(S_db_badChoice, paddr, "dbFastLinkConv(cvt_menu_st)");
        return S_db_badChoice;
    }

    if (*from < pdbMenu->nChoice) {
        strncpy(to, pdbMenu->papChoiceValue[*from], MAX_STRING_SIZE);
    }
    else {
        /* Convert out-of-range values to numeric strings */
        epicsSnprintf(to, MAX_STRING_SIZE, "%u", *from);
    }
    return 0;
}


/* Get Device to String */
static long cvt_device_st(const void *f, void *t, const dbAddr *paddr)
{
    const epicsEnum16 *from = (const epicsEnum16 *) f;
    char *to = (char *) t;
    dbFldDes             *pdbFldDes;
    dbDeviceMenu         *pdbDeviceMenu;
    char                 **papChoice;
    char                 *pchoice;

    if (!paddr
    || !(pdbFldDes = paddr->pfldDes)) {
        recGblDbaddrError(S_db_errArg, paddr, "dbFastLinkConv(cvt_device_st)");
        return S_db_errArg;
    }
    if (!(pdbDeviceMenu = (dbDeviceMenu *)pdbFldDes->ftPvt)) {
        /* Valid, record type has no device support */
        *to = '\0';
        return 0;
    }
    if (*from >= pdbDeviceMenu->nChoice
    || !(papChoice= pdbDeviceMenu->papChoice)
    || !(pchoice=papChoice[*from])) {
        recGblDbaddrError(S_db_badChoice,paddr,"dbFastLinkConv(cvt_device_st)");
        return S_db_badChoice;
    }
    strncpy(to,pchoice,MAX_STRING_SIZE);
    return 0;
}

/*
 *  Get conversion routine lookup table
 *
 *  Converts type X to ...
 *
 *  DBR_STRING,    DBR_CHR,       DBR_UCHAR,     DBR_SHORT,     DBR_USHORT,
 *  DBR_LONG,      DBR_ULONG,     DBR_INT64,     DBR_UINT64,    DBR_FLOAT,     DBR_DOUBLE,    DBR_ENUM
 *
 *  NULL implies the conversion is not supported.
 */

FASTCONVERTFUNC const dbFastGetConvertRoutine[DBF_DEVICE+1][DBR_ENUM+1] = {

 /* Convert DBF_STRING to ... */
{ cvt_st_st, cvt_st_c, cvt_st_uc, cvt_st_s, cvt_st_us, cvt_st_l, cvt_st_ul, cvt_st_q, cvt_st_uq, cvt_st_f, cvt_st_d, cvt_st_e },

 /* Convert DBF_CHAR to ... */
{ cvt_c_st, cvt_c_c, cvt_c_uc, cvt_c_s, cvt_c_us, cvt_c_l, cvt_c_ul, cvt_c_q, cvt_c_uq, cvt_c_f, cvt_c_d, cvt_c_e },

 /* Convert DBF_UCHAR to ... */
{ cvt_uc_st, cvt_uc_c, cvt_uc_uc, cvt_uc_s, cvt_uc_us, cvt_uc_l, cvt_uc_ul, cvt_uc_q, cvt_uc_uq, cvt_uc_f, cvt_uc_d, cvt_uc_e },

 /* Convert DBF_SHORT to ... */
{ cvt_s_st, cvt_s_c, cvt_s_uc, cvt_s_s, cvt_s_us, cvt_s_l, cvt_s_ul, cvt_s_q, cvt_s_uq, cvt_s_f, cvt_s_d, cvt_s_e },

 /* Convert DBF_USHORT to ... */
{ cvt_us_st, cvt_us_c, cvt_us_uc, cvt_us_s, cvt_us_us, cvt_us_l, cvt_us_ul, cvt_us_q, cvt_us_uq, cvt_us_f, cvt_us_d, cvt_us_e },

 /* Convert DBF_LONG to ... */
{ cvt_l_st, cvt_l_c, cvt_l_uc, cvt_l_s, cvt_l_us, cvt_l_l, cvt_l_ul, cvt_l_q, cvt_l_uq, cvt_l_f, cvt_l_d, cvt_l_e },

 /* Convert DBF_ULONG to ... */
{ cvt_ul_st, cvt_ul_c, cvt_ul_uc, cvt_ul_s, cvt_ul_us, cvt_ul_l, cvt_ul_ul, cvt_ul_q, cvt_ul_uq, cvt_ul_f, cvt_ul_d, cvt_ul_e },

 /* Convert DBF_INT64 to ... */
{ cvt_q_st, cvt_q_c, cvt_q_uc, cvt_q_s, cvt_q_us, cvt_q_l, cvt_q_ul, cvt_q_q, cvt_q_uq, cvt_q_f, cvt_q_d, cvt_q_e },

 /* Convert DBF_UINT64 to ... */
{ cvt_uq_st, cvt_uq_c, cvt_uq_uc, cvt_uq_s, cvt_uq_us, cvt_uq_l, cvt_uq_ul, cvt_uq_q, cvt_uq_uq, cvt_uq_f, cvt_uq_d, cvt_uq_e },

 /* Convert DBF_FLOAT to ... */
{ cvt_f_st, cvt_f_c, cvt_f_uc, cvt_f_s, cvt_f_us, cvt_f_l, cvt_f_ul, cvt_f_q, cvt_f_uq, cvt_f_f, cvt_f_d, cvt_f_e },

 /* Convert DBF_DOUBLE to ... */
{ cvt_d_st, cvt_d_c, cvt_d_uc, cvt_d_s, cvt_d_us, cvt_d_l, cvt_d_ul, cvt_d_q, cvt_d_uq, cvt_d_f, cvt_d_d, cvt_d_e },

 /* Convert DBF_ENUM to ... */
{ cvt_e_st_get, cvt_e_c, cvt_e_uc, cvt_e_s, cvt_e_us, cvt_e_l, cvt_e_ul, cvt_e_q, cvt_e_uq, cvt_e_f, cvt_e_d, cvt_e_e },

 /* Convert DBF_MENU to ... */
{ cvt_menu_st, cvt_e_c, cvt_e_uc, cvt_e_s, cvt_e_us, cvt_e_l, cvt_e_ul, cvt_e_q, cvt_e_uq, cvt_e_f, cvt_e_d, cvt_e_e },

 /* Convert DBF_DEVICE to ... */
{ cvt_device_st, cvt_e_c, cvt_e_uc, cvt_e_s, cvt_e_us, cvt_e_l, cvt_e_ul, cvt_e_q, cvt_e_uq, cvt_e_f, cvt_e_d, cvt_e_e } };

/*
 *  Put conversion routine lookup table
 *
 *  Converts type X to ...
 *
 *  DBF_STRING     DBF_CHAR       DBF_UCHAR      DBF_SHORT      DBF_USHORT
 *  DBF_LONG       DBF_ULONG      DBF_INT64      DBF_UINT64     DBF_FLOAT      DBF_DOUBLE     DBF_ENUM
 *  DBF_MENU       DBF_DEVICE
 *
 *  NULL implies the conversion is not supported.
 */

FASTCONVERTFUNC const dbFastPutConvertRoutine[DBR_ENUM+1][DBF_DEVICE+1] = {

 /* Convert DBR_STRING to ... */
{ cvt_st_st, cvt_st_c, cvt_st_uc, cvt_st_s, cvt_st_us, cvt_st_l, cvt_st_ul, cvt_st_q, cvt_st_uq, cvt_st_f, cvt_st_d, cvt_st_e, cvt_st_menu, cvt_st_device},

 /* Convert DBR_CHAR to ... */
{ cvt_c_st, cvt_c_c, cvt_c_uc, cvt_c_s, cvt_c_us, cvt_c_l, cvt_c_ul, cvt_c_q, cvt_c_uq, cvt_c_f, cvt_c_d, cvt_c_e, cvt_c_e, cvt_c_e},

 /* Convert DBR_UCHAR to ... */
{ cvt_uc_st, cvt_uc_c, cvt_uc_uc, cvt_uc_s, cvt_uc_us, cvt_uc_l, cvt_uc_ul, cvt_uc_q, cvt_uc_uq, cvt_uc_f, cvt_uc_d, cvt_uc_e, cvt_uc_e, cvt_uc_e},

 /* Convert DBR_SHORT to ... */
{ cvt_s_st, cvt_s_c, cvt_s_uc, cvt_s_s, cvt_s_us, cvt_s_l, cvt_s_ul, cvt_s_q, cvt_s_uq, cvt_s_f, cvt_s_d, cvt_s_e, cvt_s_e, cvt_s_e},

 /* Convert DBR_USHORT to ... */
{ cvt_us_st, cvt_us_c, cvt_us_uc, cvt_us_s, cvt_us_us, cvt_us_l, cvt_us_ul, cvt_us_q, cvt_us_uq, cvt_us_f, cvt_us_d, cvt_us_e, cvt_us_e, cvt_us_e},

 /* Convert DBR_LONG to ... */
{ cvt_l_st, cvt_l_c, cvt_l_uc, cvt_l_s, cvt_l_us, cvt_l_l, cvt_l_ul, cvt_l_q, cvt_l_uq, cvt_l_f, cvt_l_d, cvt_l_e, cvt_l_e, cvt_l_e},

 /* Convert DBR_ULONG to ... */
{ cvt_ul_st, cvt_ul_c, cvt_ul_uc, cvt_ul_s, cvt_ul_us, cvt_ul_l, cvt_ul_ul, cvt_ul_q, cvt_ul_uq, cvt_ul_f, cvt_ul_d, cvt_ul_e, cvt_ul_e, cvt_ul_e},

 /* Convert DBR_INT64 to ... */
{ cvt_q_st, cvt_q_c, cvt_q_uc, cvt_q_s, cvt_q_us, cvt_q_l, cvt_q_ul, cvt_q_q, cvt_q_uq, cvt_q_f, cvt_q_d, cvt_q_e, cvt_q_e, cvt_q_e},

 /* Convert DBR_UINT64 to ... */
{ cvt_uq_st, cvt_uq_c, cvt_uq_uc, cvt_uq_s, cvt_uq_us, cvt_uq_l, cvt_uq_ul, cvt_uq_q, cvt_uq_uq, cvt_uq_f, cvt_uq_d, cvt_uq_e, cvt_uq_e, cvt_uq_e},

 /* Convert DBR_FLOAT to ... */
{ cvt_f_st, cvt_f_c, cvt_f_uc, cvt_f_s, cvt_f_us, cvt_f_l, cvt_f_ul, cvt_f_q, cvt_f_uq, cvt_f_f, cvt_f_d, cvt_f_e, cvt_f_e, cvt_f_e},

 /* Convert DBR_DOUBLE to ... */
{ cvt_d_st, cvt_d_c, cvt_d_uc, cvt_d_s, cvt_d_us, cvt_d_l, cvt_d_ul, cvt_d_q, cvt_d_uq, cvt_d_f, cvt_d_d, cvt_d_e, cvt_d_e, cvt_d_e},

 /* Convert DBR_ENUM to ... */
{ cvt_e_st_put, cvt_e_c, cvt_e_uc, cvt_e_s, cvt_e_us, cvt_e_l, cvt_e_ul, cvt_e_q, cvt_e_uq, cvt_e_f, cvt_e_d, cvt_e_e, cvt_e_e, cvt_e_e} };

