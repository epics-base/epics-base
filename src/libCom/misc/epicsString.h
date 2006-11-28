/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*epicsString.h*/
/*Authors: Jun-ichi Odagiri, Marty Kraimer, Eric Norum, Mark Rivers*/

/* int dbTranslateEscape(char *s,const char *ct);
 *
 * copies ct to s while substituting escape sequences
 * returns the length of the resultant string (may contain nulls)
*/

#ifndef INC_epicsString_H
#define INC_epicsString_H

#include <stdio.h>
#include <shareLib.h>

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc int dbTranslateEscape(char *s,const char *ct);
epicsShareFunc int epicsStrCaseCmp(const char *s1,const char *s2);
epicsShareFunc int epicsStrnCaseCmp(const char *s1,const char *s2, int n);
epicsShareFunc char * epicsStrDup(const char *s);
epicsShareFunc int epicsStrPrintEscaped(
    FILE *fp, const char *s, int n);
epicsShareFunc int epicsStrSnPrintEscaped(
    char *outbuf, int outsize, const char *inbuf, int inlen);
epicsShareFunc int epicsStrGlobMatch(
    const char *str, const char *pattern);
epicsShareFunc char * epicsStrtok_r(
    char *s, const char *delim, char **lasts);

#ifdef __cplusplus
}
#endif

#endif /* INC_epicsString_H */
