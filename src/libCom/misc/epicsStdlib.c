/*************************************************************************\
* Copyright (c) 2009 UChicago Argonna LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*epicsStdlib.c*/
/*Author: Eric Norum */

#include <ctype.h>
#include <stdio.h>

#define epicsExportSharedSymbols
#include "epicsMath.h"
#include "epicsStdlib.h"
#include "epicsString.h"


epicsShareFunc int epicsScanDouble(const char *str, double *dest)
{
    char *endp;
    double dtmp;

    dtmp = epicsStrtod(str, &endp);
    if (endp == str)
        return 0;
    *dest = dtmp;
    return 1;
}

epicsShareFunc int epicsScanFloat(const char *str, float *dest)
{
    char *endp;
    double dtmp;

    dtmp = epicsStrtod(str, &endp);
    if (endp == str)
        return 0;
    *dest = (float)dtmp;
    return 1;
}

/* Systems with a working strtod() just #define epicsStrtod strtod */
#ifndef epicsStrtod
epicsShareFunc double epicsStrtod(const char *str, char **endp)
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
    if (!isalpha((int)*cp))
        return strtod(str, endp);

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
