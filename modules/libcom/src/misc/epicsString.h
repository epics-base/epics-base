/*************************************************************************\
* Copyright (c) 2009 Helmholtz-Zentrum Berlin fuer Materialien und Energie.
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* Authors: Jun-ichi Odagiri, Marty Kraimer, Eric Norum,
 *          Mark Rivers, Andrew Johnson, Ralph Lange
 */

#ifndef INC_epicsString_H
#define INC_epicsString_H

#include <stdio.h>
#include "epicsTypes.h"
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc int epicsStrnRawFromEscaped(char *outbuf,      size_t outsize,
                                           const char *inbuf, size_t inlen);
epicsShareFunc int epicsStrnEscapedFromRaw(char *outbuf,      size_t outsize,
                                           const char *inbuf, size_t inlen);
epicsShareFunc size_t epicsStrnEscapedFromRawSize(const char *buf, size_t len);
epicsShareFunc int epicsStrCaseCmp(const char *s1, const char *s2);
epicsShareFunc int epicsStrnCaseCmp(const char *s1, const char *s2, size_t len);
epicsShareFunc char * epicsStrDup(const char *s);
epicsShareFunc char * epicsStrnDup(const char *s, size_t len);
epicsShareFunc int epicsStrPrintEscaped(FILE *fp, const char *s, size_t n);
#define epicsStrSnPrintEscaped epicsStrnEscapedFromRaw
epicsShareFunc size_t epicsStrnLen(const char *s, size_t maxlen);
epicsShareFunc int epicsStrGlobMatch(const char *str, const char *pattern);
epicsShareFunc char * epicsStrtok_r(char *s, const char *delim, char **lasts);
epicsShareFunc unsigned int epicsStrHash(const char *str, unsigned int seed);
epicsShareFunc unsigned int epicsMemHash(const char *str, size_t length,
                                         unsigned int seed);

/* dbTranslateEscape is deprecated, use epicsStrnRawFromEscaped instead */
epicsShareFunc int dbTranslateEscape(char *s, const char *ct);

#ifdef __cplusplus
}
#endif

#endif /* INC_epicsString_H */
