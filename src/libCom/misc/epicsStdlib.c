/*************************************************************************\
* Copyright (c) 2012 UChicago Argonna LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* Authors: Eric Norum & Andrew Johnson */

#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <float.h>

#define epicsExportSharedSymbols
#include "epicsMath.h"
#include "epicsStdlib.h"
#include "epicsString.h"
#include "epicsConvert.h"


/* These are the conversion primitives */

epicsShareFunc int
epicsParseLong(const char *str, long *to, int base, char **units)
{
    int c;
    char *endp;
    long value;

    while ((c = *str) && isspace(c))
        ++str;

    errno = 0;
    value = strtol(str, &endp, base);

    if (endp == str)
        return S_stdlib_noConversion;
    if (errno == EINVAL)    /* Not universally supported */
        return S_stdlib_badBase;
    if (errno == ERANGE)
        return S_stdlib_overflow;

    while ((c = *endp) && isspace(c))
        ++endp;
    if (c && !units)
        return S_stdlib_extraneous;

    *to = value;
    if (units)
        *units = endp;
    return 0;
}

epicsShareFunc int
epicsParseULong(const char *str, unsigned long *to, int base, char **units)
{
    int c;
    char *endp;
    unsigned long value;

    while ((c = *str) && isspace(c))
        ++str;

    errno = 0;
    value = strtoul(str, &endp, base);

    if (endp == str)
        return S_stdlib_noConversion;
    if (errno == EINVAL)    /* Not universally supported */
        return S_stdlib_badBase;
    if (errno == ERANGE)
        return S_stdlib_overflow;

    while ((c = *endp) && isspace(c))
        ++endp;
    if (c && !units)
        return S_stdlib_extraneous;

    *to = value;
    if (units)
        *units = endp;
    return 0;
}

epicsShareFunc int
epicsParseLLong(const char *str, long long *to, int base, char **units)
{
    int c;
    char *endp;
    long long value;

    while ((c = *str) && isspace(c))
        ++str;

    errno = 0;
    value = strtoll(str, &endp, base);

    if (endp == str)
        return S_stdlib_noConversion;
    if (errno == EINVAL)    /* Not universally supported */
        return S_stdlib_badBase;
    if (errno == ERANGE)
        return S_stdlib_overflow;

    while ((c = *endp) && isspace(c))
        ++endp;
    if (c && !units)
        return S_stdlib_extraneous;

    *to = value;
    if (units)
        *units = endp;
    return 0;
}

epicsShareFunc int
epicsParseULLong(const char *str, unsigned long long *to, int base, char **units)
{
    int c;
    char *endp;
    unsigned long long value;

    while ((c = *str) && isspace(c))
        ++str;

    errno = 0;
    value = strtoull(str, &endp, base);

    if (endp == str)
        return S_stdlib_noConversion;
    if (errno == EINVAL)    /* Not universally supported */
        return S_stdlib_badBase;
    if (errno == ERANGE)
        return S_stdlib_overflow;

    while ((c = *endp) && isspace(c))
        ++endp;
    if (c && !units)
        return S_stdlib_extraneous;

    *to = value;
    if (units)
        *units = endp;
    return 0;
}

epicsShareFunc int
epicsParseDouble(const char *str, double *to, char **units)
{
    int c;
    char *endp;
    double value;

    while ((c = *str) && isspace(c))
        ++str;

    errno = 0;
    value = epicsStrtod(str, &endp);

    if (endp == str)
        return S_stdlib_noConversion;
    if (errno == ERANGE)
        return (value == 0) ? S_stdlib_underflow : S_stdlib_overflow;

    while ((c = *endp) && isspace(c))
        ++endp;
    if (c && !units)
        return S_stdlib_extraneous;

    *to = value;
    if (units)
        *units = endp;
    return 0;
}


/* These call the primitives */

epicsShareFunc int
epicsParseInt8(const char *str, epicsInt8 *to, int base, char **units)
{
    long value;
    int status = epicsParseLong(str, &value, base, units);

    if (status)
        return status;

    if (value < -0x80 || value > 0x7f)
        return S_stdlib_overflow;

    *to = (epicsInt8) value;
    return 0;
}

epicsShareFunc int
epicsParseUInt8(const char *str, epicsUInt8 *to, int base, char **units)
{
    unsigned long value;
    int status = epicsParseULong(str, &value, base, units);

    if (status)
        return status;

    if (value > 0xff && value <= ~0xffUL)
        return S_stdlib_overflow;

    *to = (epicsUInt8) value;
    return 0;
}

epicsShareFunc int
epicsParseInt16(const char *str, epicsInt16 *to, int base, char **units)
{
    long value;
    int status = epicsParseLong(str, &value, base, units);

    if (status)
        return status;

    if (value < -0x8000 || value > 0x7fff)
        return S_stdlib_overflow;

    *to = (epicsInt16) value;
    return 0;
}

epicsShareFunc int
epicsParseUInt16(const char *str, epicsUInt16 *to, int base, char **units)
{
    unsigned long value;
    int status = epicsParseULong(str, &value, base, units);

    if (status)
        return status;

    if (value > 0xffff && value <= ~0xffffUL)
        return S_stdlib_overflow;

    *to = (epicsUInt16) value;
    return 0;
}

epicsShareFunc int
epicsParseInt32(const char *str, epicsInt32 *to, int base, char **units)
{
    long value;
    int status = epicsParseLong(str, &value, base, units);

    if (status)
        return status;

#if (LONG_MAX > 0x7fffffffLL)
    if (value < -0x80000000L || value > 0x7fffffffL)
        return S_stdlib_overflow;
#endif

    *to = (epicsInt32) value;
    return 0;
}

epicsShareFunc int
epicsParseUInt32(const char *str, epicsUInt32 *to, int base, char **units)
{
    unsigned long value;
    int status = epicsParseULong(str, &value, base, units);

    if (status)
        return status;

#if (ULONG_MAX > 0xffffffffULL)
    if (value > 0xffffffffUL && value <= ~0xffffffffUL)
        return S_stdlib_overflow;
#endif

    *to = (epicsUInt32) value;
    return 0;
}

epicsShareFunc int
epicsParseInt64(const char *str, epicsInt64 *to, int base, char **units)
{
#if (LONG_MAX == 0x7fffffffffffffffLL)
    long value;
    int status = epicsParseLong(str, &value, base, units);
#else
    long long value;
    int status = epicsParseLLong(str, &value, base, units);
#endif

    if (status)
        return status;

    *to = (epicsInt64) value;
    return 0;
}

epicsShareFunc int
epicsParseUInt64(const char *str, epicsUInt64 *to, int base, char **units)
{
#if (ULONG_MAX == 0xffffffffffffffffULL)
    unsigned long value;
    int status = epicsParseULong(str, &value, base, units);
#else
    unsigned long long value;
    int status = epicsParseULLong(str, &value, base, units);
#endif

    if (status)
        return status;

    *to = (epicsUInt64) value;
    return 0;
}


epicsShareFunc int
epicsParseFloat(const char *str, float *to, char **units)
{
    double value, abs;
    int status = epicsParseDouble(str, &value, units);

    if (status)
        return status;

    abs = fabs(value);
    if (value > 0 && abs <= FLT_MIN)
        return S_stdlib_underflow;
    if (finite(value) && abs >= FLT_MAX)
        return S_stdlib_overflow;

    *to = (float) value;
    return 0;
}


/* If strtod() works properly, the OS-specific osdStrtod.h does:
 *   #define epicsStrtod strtod
 *
 * If strtod() is broken, osdStrtod.h defines this prototype:
 *   epicsShareFunc double epicsStrtod(const char *str, char **endp);
 */

#ifndef epicsStrtod
epicsShareFunc double
epicsStrtod(const char *str, char **endp)
{
    const char *cp = str;
    int negative = 0;
    double res;

    while (isspace((int)*cp))
        cp++;

    if (*cp == '+') {
        cp++;
    } else if (*cp == '-') {
        negative = 1;
        cp++;
    }

    if (epicsStrnCaseCmp("0x", cp, 2) == 0) {
        if (negative)
            return strtol(str, endp, 16);
        else
            return strtoul(str, endp, 16);
    }
    if (!isalpha((int)*cp)) {
        res = strtod(str, endp);
        if (isinf(res))
            errno = ERANGE;
        return res;
    }

    if (epicsStrnCaseCmp("NAN", cp, 3) == 0) {
        res = epicsNAN;
        cp += 3;
        if (*cp == '(') {
            cp++;
            while (*cp && (*cp++ != ')'))
                continue;
        }
    }
    else if (epicsStrnCaseCmp("INF", cp, 3) == 0) {
        res = negative ? -epicsINF : epicsINF;
        cp += 3;
        if (epicsStrnCaseCmp("INITY", cp, 5) == 0) {
            cp += 5;
        }
    } else {
        cp = str;
        res = 0;
    }

    if (endp)
        *endp = (char *)cp;

    return res;
}
#endif
