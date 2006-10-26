/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*epicsStdlib.c*/
/*Author: Eric Norum */

#include <ctype.h>
#include <math.h>
#include <stdio.h>

#define epicsExportSharedSymbols
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
    *dest = dtmp;
    return 1;
}

/* Systems with a working strtod() just #define epicsStrtod strtod */
#ifndef epicsStrtod
epicsShareFunc double epicsStrtod( 
    const char *str, char **endp)
{
    const char *cp = str;
    double num = 1.0;
    double den = 0.0;

    while (isspace((int)*cp))
        cp++;
    if (*cp == '+') {
        cp++;
    }
    else if (*cp == '-') {
        num = -1.0;
        cp++;
    }
    if (!isalpha((int)*cp))
        return strtod(str, endp);
    if (epicsStrnCaseCmp("NAN", cp, 3) == 0) {
        num = 0.0;
        cp += 3;
        if (*cp == '(') {
            cp++;
            while (*cp && (*cp++ != ')'))
                continue;
        }
    }
    else if (epicsStrnCaseCmp("INF", cp, 3) == 0) {
        cp += 3;
        if (epicsStrnCaseCmp("INITY", cp, 5) == 0) {
            cp += 5;
        }
    }
    else {
        cp = str;
        num = 0.0;
        den = 1.0;
    }
    if (endp)
        *endp = (char *)cp;
    return num / den;
}
#endif
