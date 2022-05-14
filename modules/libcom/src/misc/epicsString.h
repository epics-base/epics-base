/*************************************************************************\
* Copyright (c) 2009 Helmholtz-Zentrum Berlin fuer Materialien und Energie.
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
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
#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

LIBCOM_API int epicsStrnRawFromEscaped(char *outbuf,      size_t outsize,
                                           const char *inbuf, size_t inlen);
LIBCOM_API int epicsStrnEscapedFromRaw(char *outbuf,      size_t outsize,
                                           const char *inbuf, size_t inlen);
LIBCOM_API size_t epicsStrnEscapedFromRawSize(const char *buf, size_t len);
LIBCOM_API int epicsStrCaseCmp(const char *s1, const char *s2);
LIBCOM_API int epicsStrnCaseCmp(const char *s1, const char *s2, size_t len);
LIBCOM_API char * epicsStrDup(const char *s);
LIBCOM_API char * epicsStrnDup(const char *s, size_t len);
LIBCOM_API int epicsStrPrintEscaped(FILE *fp, const char *s, size_t n);
#define epicsStrSnPrintEscaped epicsStrnEscapedFromRaw
LIBCOM_API size_t epicsStrnLen(const char *s, size_t maxlen);

/** Matches a string against a pattern.
 *
 * Checks if str matches the glob style pattern, which may contain ? or * wildcards.
 * A ? matches any single character.
 * A * matched any sub-string.
 *
 * @returns 1 if str matches the pattern, 0 if not.
 *
 * @since EPICS 3.14.7
 */
LIBCOM_API int epicsStrGlobMatch(const char *str, const char *pattern);

/** Matches a string against a pattern.
 *
 * Like epicsStrGlobMatch but with limited string length.
 * If the length of str is less than len, the full string is matched.
 *
 * @returns 1 if the first len characters of str match the pattern, 0 if not.
 *
 * @since 7.0.6
 */
LIBCOM_API int epicsStrnGlobMatch(const char *str, size_t len, const char *pattern);

LIBCOM_API char * epicsStrtok_r(char *s, const char *delim, char **lasts);
LIBCOM_API unsigned int epicsStrHash(const char *str, unsigned int seed);
LIBCOM_API unsigned int epicsMemHash(const char *str, size_t length,
                                         unsigned int seed);
/** Compare two strings and return a number in the range [0.0, 1.0] or -1.0 on error.
 *
 * Computes a normalized edit distance representing the similarity between two strings.
 *
 * @returns 1.0 when A and B are identical, down to 0.0 when A and B are unrelated,
 *          or < 0.0 on error.
 *
 * @since EPICS 7.0.5
 */
LIBCOM_API double epicsStrSimilarity(const char *A, const char *B);

/* dbTranslateEscape is deprecated, use epicsStrnRawFromEscaped instead */
LIBCOM_API int dbTranslateEscape(char *s, const char *ct);

#ifdef __cplusplus
}
#endif

#endif /* INC_epicsString_H */
