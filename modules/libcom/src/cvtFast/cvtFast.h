/*************************************************************************\
* Copyright (c) 2013 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/**
 * \file cvtFast.h
 * \author Bob Dalesio, Mark Anderson, Marty Kraimer
 *
 * \brief Fast numeric to string conversions
 *
 * \details
 * Provides routines for converting various numeric types into an ascii string.
 * They off a combination of speed and convenience not available with sprintf().
 *
 * All functions return the number of characters in the output 
 */

#ifndef INCcvtFasth
#define INCcvtFasth

#include <stddef.h>

#include "epicsTypes.h"
#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

LIBCOM_API int
    cvtFloatToString(float val, char *pdest, epicsUInt16 prec);
LIBCOM_API int
    cvtDoubleToString(double val, char *pdest, epicsUInt16 prec);

LIBCOM_API int
    cvtFloatToExpString(float val, char *pdest, epicsUInt16 prec);
LIBCOM_API int
    cvtDoubleToExpString(double val, char *pdest, epicsUInt16 prec);
LIBCOM_API int
    cvtFloatToCompactString(float val, char *pdest, epicsUInt16 prec);
LIBCOM_API int
    cvtDoubleToCompactString(double val, char *pdest, epicsUInt16 prec);

LIBCOM_API size_t
    cvtInt32ToString(epicsInt32 val, char *pdest);
LIBCOM_API size_t
    cvtUInt32ToString(epicsUInt32 val, char *pdest);
LIBCOM_API size_t
    cvtInt64ToString(epicsInt64 val, char *pdest);
LIBCOM_API size_t
    cvtUInt64ToString(epicsUInt64 val, char *pdest);

LIBCOM_API size_t
    cvtInt32ToHexString(epicsInt32 val, char *pdest);
LIBCOM_API size_t
    cvtUInt32ToHexString(epicsUInt32 val, char *pdest);
LIBCOM_API size_t
    cvtInt32ToOctalString(epicsInt32 val, char *pdest);
LIBCOM_API size_t
    cvtInt64ToHexString(epicsInt64 val, char *pdest);
LIBCOM_API size_t
    cvtUInt64ToHexString(epicsUInt64 val, char *pdest);

/* Support the original names */

#define cvtCharToString(val, str) cvtInt32ToString(val, str)
#define cvtUcharToString(val, str) cvtUInt32ToString(val, str)
#define cvtShortToString(val, str) cvtInt32ToString(val, str)
#define cvtUshortToString(val, str) cvtUInt32ToString(val, str)
#define cvtLongToString(val, str) cvtInt32ToString(val, str)
#define cvtUlongToString(val, str) cvtUInt32ToString(val, str)

#define cvtLongToHexString(val, str) cvtInt32ToHexString(val, str)
#define cvtULongToHexString(val, str) cvtUInt32ToHexString(val, str)
#define cvtLongToOctalString(val, str) cvtInt32ToOctalString(val, str)

#ifdef __cplusplus
}
#endif

#endif /*INCcvtFasth*/
