/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*epicsStrtod.c*/
/*Author: Eric Norum */

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include "epicsString.h"

#define epicsExportSharedSymbols
#include "epicsStdlib.h"

#ifdef epicsStrtod
# undef epicsStrtod
#endif

epicsShareFunc double epicsShareAPI epicsStrtod( 
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
    }
    else if (epicsStrnCaseCmp("INFINITY", cp, 8) == 0) {
        cp += 8;
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
