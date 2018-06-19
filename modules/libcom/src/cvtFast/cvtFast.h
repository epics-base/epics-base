/*************************************************************************\
* Copyright (c) 2013 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 * Fast numeric to string conversions
 *
 * Original Authors:
 *    Bob Dalesio, Mark Anderson and Marty Kraimer
 *    Date:            12 January 1993
 */

#ifndef INCcvtFasth
#define INCcvtFasth

#include <stddef.h>

#include "epicsTypes.h"
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * All functions return the number of characters in the output
 */
epicsShareFunc int
    cvtFloatToString(float val, char *pdest, epicsUInt16 prec);
epicsShareFunc int
    cvtDoubleToString(double val, char *pdest, epicsUInt16 prec);

epicsShareFunc int
    cvtFloatToExpString(float val, char *pdest, epicsUInt16 prec);
epicsShareFunc int
    cvtDoubleToExpString(double val, char *pdest, epicsUInt16 prec);
epicsShareFunc int
    cvtFloatToCompactString(float val, char *pdest, epicsUInt16 prec);
epicsShareFunc int
    cvtDoubleToCompactString(double val, char *pdest, epicsUInt16 prec);

epicsShareFunc size_t
    cvtInt32ToString(epicsInt32 val, char *pdest);
epicsShareFunc size_t
    cvtUInt32ToString(epicsUInt32 val, char *pdest);
epicsShareFunc size_t
    cvtInt64ToString(epicsInt64 val, char *pdest);
epicsShareFunc size_t
    cvtUInt64ToString(epicsUInt64 val, char *pdest);

epicsShareFunc size_t
    cvtInt32ToHexString(epicsInt32 val, char *pdest);
epicsShareFunc size_t
    cvtUInt32ToHexString(epicsUInt32 val, char *pdest);
epicsShareFunc size_t
    cvtInt32ToOctalString(epicsInt32 val, char *pdest);
epicsShareFunc size_t
    cvtInt64ToHexString(epicsInt64 val, char *pdest);
epicsShareFunc size_t
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
