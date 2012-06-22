/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* epicsStdlib.h */
/* Author: Eric Norum */

#include <stdlib.h>
#include <limits.h>

#include "shareLib.h"
#include "osdStrtod.h"
#include "epicsTypes.h"
#include "errMdef.h"

#ifdef __cplusplus
extern "C" {
#endif

#define S_stdlib_noConversion (M_stdlib | 1) /* No digits to convert */
#define S_stdlib_extraneous   (M_stdlib | 2) /* Extraneous characters */
#define S_stdlib_underflow    (M_stdlib | 3) /* Too small to represent */
#define S_stdlib_overflow     (M_stdlib | 4) /* Too large to represent */
#define S_stdlib_badBase      (M_stdlib | 5) /* Number base not supported */


epicsShareFunc int
    epicsParseLong(const char *str, long *to, int base);
epicsShareFunc int
    epicsParseULong(const char *str, unsigned long *to, int base);
epicsShareFunc int
    epicsParseDouble(const char *str, double *to);

epicsShareFunc int
    epicsParseFloat(const char *str, float *to);

epicsShareFunc int
    epicsParseInt8(const char *str, epicsInt8 *to, int base);
epicsShareFunc int
    epicsParseUInt8(const char *str, epicsUInt8 *to, int base);
epicsShareFunc int
    epicsParseInt16(const char *str, epicsInt16 *to, int base);
epicsShareFunc int
    epicsParseUInt16(const char *str, epicsUInt16 *to, int base);

epicsShareFunc int
    epicsParseInt32(const char *str, epicsInt32 *to, int base);
epicsShareFunc int
    epicsParseUInt32(const char *str, epicsUInt32 *to, int base);

#define epicsParseFloat32(str, to) epicsParseFloat(str, to)
#define epicsParseFloat64(str, to) epicsParseDouble(str, to)

/* These macros return 1 if successful, 0 on failure.
 * This is analagous to the return value from sscanf()
 */
#define epicsScanLong(str, to, base) !epicsParseLong(str, to, base)
#define epicsScanULong(str, to, base) !epicsParseULong(str, to, base)
#define epicsScanFloat(str, to) !epicsParseFloat(str, to)
#define epicsScanDouble(str, to) !epicsParseDouble(str, to)

#ifdef __cplusplus
}
#endif

