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
 *
 * Routines in this file should have corresponding test code in
 *    libCom/test/epicsStringTest.c
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>

#ifndef vxWorks
#include <stdint.h>
#else
/* VxWorks automatically includes stdint.h defining SIZE_MAX in 6.9 but not earlier */
#ifndef SIZE_MAX
#define SIZE_MAX (size_t)-1
#endif
#endif

#include "epicsAssert.h"
#include "epicsStdio.h"
#include "cantProceed.h"
#include "epicsString.h"
#include "epicsMath.h"

/* Deprecated, use epicsStrnRawFromEscaped() instead */
int dbTranslateEscape(char *dst, const char *src)
{
    size_t big_enough = strlen(src) + 1;

    return epicsStrnRawFromEscaped(dst, big_enough, src, big_enough);
}

int epicsStrnRawFromEscaped(char *dst, size_t dstlen, const char *src,
    size_t srclen)
{
    int rem = dstlen;
    int ndst = 0;

    while (srclen--) {
        int c = *src++;
        #define OUT(chr) if (--rem > 0) ndst++, *dst++ = chr

        if (!c) break;

    input:
        if (c != '\\') {
            OUT(c);
            continue;
        }

        if (!srclen-- || !(c = *src++)) break;

        switch (c) {
        case 'a':  OUT('\a'); break;
        case 'b':  OUT('\b'); break;
        case 'f':  OUT('\f'); break;
        case 'n':  OUT('\n'); break;
        case 'r':  OUT('\r'); break;
        case 't':  OUT('\t'); break;
        case 'v':  OUT('\v'); break;
        case '\\': OUT('\\'); break;
        case '\'': OUT('\''); break;
        case '\"': OUT('\"'); break;
        case '0':  OUT('\0'); break;

        case 'x' :
            { /* \xXX */
                unsigned int u = 0;

                if (!srclen-- || !(c = *src++ & 0xff))
                    goto done;

                if (!isxdigit(c))
                    goto input;

                u = u << 4 | ((c > '9') ? toupper(c) - 'A' + 10 : c - '0');

                if (!srclen-- || !(c = *src++ & 0xff)) {
                    OUT(u);
                    goto done;
                }

                if (!isxdigit(c)) {
                    OUT(u);
                    goto input;
                }

                u = u << 4 | ((c > '9') ? toupper(c) - 'A' + 10 : c - '0');
                OUT(u);
            }
            break;

        default:
            OUT(c);
        }
        #undef OUT
    }
done:
    if (dstlen)
        *dst = '\0';
    return ndst;
}

int epicsStrnEscapedFromRaw(char *dst, size_t dstlen, const char *src,
    size_t srclen)
{
    int rem = dstlen;
    int ndst = 0;

    if (dst == src)
        return -1;

    while (srclen--) {
        static const char hex[] = "0123456789abcdef";
        int c = *src++;
        #define OUT(chr) ndst++; if (--rem > 0) *dst++ = chr

        switch (c) {
        case '\a': OUT('\\'); OUT('a');  break;
        case '\b': OUT('\\'); OUT('b');  break;
        case '\f': OUT('\\'); OUT('f');  break;
        case '\n': OUT('\\'); OUT('n');  break;
        case '\r': OUT('\\'); OUT('r');  break;
        case '\t': OUT('\\'); OUT('t');  break;
        case '\v': OUT('\\'); OUT('v');  break;
        case '\\': OUT('\\'); OUT('\\'); break;
        case '\'': OUT('\\'); OUT('\''); break;
        case '\"': OUT('\\'); OUT('\"'); break;
        case '\0': OUT('\\'); OUT('0');  break;
        default:
            if (isprint(c & 0xff)) {
                OUT(c);
                break;
            }
            OUT('\\'); OUT('x');
            OUT(hex[(c >> 4) & 0x0f]);
            OUT(hex[ c       & 0x0f]);
        }
        #undef OUT
    }
    if (dstlen)
        *dst = '\0';
    return ndst;
}

size_t epicsStrnEscapedFromRawSize(const char *src, size_t srclen)
{
    size_t ndst = srclen;

    while (srclen--) {
        int c = *src++;

        switch (c) {
        case '\a': case '\b': case '\f': case '\n':
        case '\r': case '\t': case '\v': case '\\':
        case '\'': case '\"': case '\0':
            ndst++;
            break;
        default:
            if (!isprint(c & 0xff))
                ndst += 3;
        }
    }
    return ndst;
}

int epicsStrCaseCmp(const char *s1, const char *s2)
{
    while (1) {
        int ch1 = toupper((int) *s1);
        int ch2 = toupper((int) *s2);

        if (ch2 == 0) return (ch1 != 0);
        if (ch1 == 0) return -1;
        if (ch1 < ch2) return -1;
        if (ch1 > ch2) return 1;
        s1++;
        s2++;
    }
}

int epicsStrnCaseCmp(const char *s1, const char *s2, size_t len)
{
    size_t i = 0;

    while (i++ < len) {
        int ch1 = toupper((int) *s1);
        int ch2 = toupper((int) *s2);

        if (ch2 == 0) return (ch1 != 0);
        if (ch1 == 0) return -1;
        if (ch1 < ch2) return -1;
        if (ch1 > ch2) return 1;
        s1++;
        s2++;
    }
    return 0;
}

char * epicsStrnDup(const char *s, size_t len)
{
    char *buf = mallocMustSucceed(len + 1, "epicsStrnDup");

    strncpy(buf, s, len);
    buf[len] = '\0';
    return buf;
}

char * epicsStrDup(const char *s)
{
    return strcpy(mallocMustSucceed(strlen(s)+1, "epicsStrDup"), s);
}

int epicsStrPrintEscaped(FILE *fp, const char *s, size_t len)
{
   int nout = 0;

   if (fp == NULL)
       return -1;

   if (s == NULL || strlen(s) == 0 || len == 0)
       return 0; // No work to do

   while (len--) {
       char c = *s++;
       int rc = 0;

       switch (c) {
       case '\a':  rc = fprintf(fp, "\\a");  break;
       case '\b':  rc = fprintf(fp, "\\b");  break;
       case '\f':  rc = fprintf(fp, "\\f");  break;
       case '\n':  rc = fprintf(fp, "\\n");  break;
       case '\r':  rc = fprintf(fp, "\\r");  break;
       case '\t':  rc = fprintf(fp, "\\t");  break;
       case '\v':  rc = fprintf(fp, "\\v");  break;
       case '\\':  rc = fprintf(fp, "\\\\"); break;
       case '\'':  rc = fprintf(fp, "\\'");  break;
       case '\"':  rc = fprintf(fp, "\\\"");  break;
       default:
           if (isprint(0xff & (int)c))
               rc = fprintf(fp, "%c", c);
           else
               rc = fprintf(fp, "\\x%02x", (unsigned char)c);
           break;
       }
       if (rc < 0) {
           return rc;
       } else {
           nout += rc;
       }
   }
   return nout;
}

/* Until Base requires POSIX 2008 we must provide our own implementation */
size_t epicsStrnLen(const char *s, size_t maxlen)
{
    size_t i;

    for (i=0; i<maxlen; i++) {
        if(s[i]=='\0')
            return i;
    }
    return i;
}

int epicsStrnGlobMatch(const char *str, size_t len, const char *pattern)
{
    const char *mp = NULL;
    size_t cp = 0, i = 0;

    while ((i < len) && (str[i]) && (*pattern != '*')) {
        if ((*pattern != str[i]) && (*pattern != '?'))
            return 0;
        pattern++;
        i++;
    }
    while ((i < len) && str[i]) {
        if (*pattern == '*') {
            if (!*++pattern)
                return 1;
            mp = pattern;
            cp = i+1;
        }
        else if ((*pattern == str[i]) || (*pattern == '?')) {
            pattern++;
            i++;
        }
        else {
            pattern = mp;
            i = cp++;
        }
    }
    while (*pattern == '*')
        pattern++;
    return !*pattern;
}

int epicsStrGlobMatch(const char *str, const char *pattern) {
    return epicsStrnGlobMatch(str, SIZE_MAX, pattern);
}

char * epicsStrtok_r(char *s, const char *delim, char **lasts)
{
   const char *spanp;
   int c, sc;
   char *tok;

   if (s == NULL && (s = *lasts) == NULL)
      return NULL;

   /*
    * Skip (span) leading delimiters (s += strspn(s, delim), sort of).
    */
cont:
   c = *s++;
   for (spanp = delim; (sc = *spanp++) != 0;) {
      if (c == sc)
         goto cont;
   }

   if (c == 0) {      /* no non-delimiter characters */
      *lasts = NULL;
      return NULL;
   }
   tok = s - 1;

   /*
    * Scan token (scan for delimiters: s += strcspn(s, delim), sort of).
    * Note that delim must have one NUL; we stop if we see that, too.
    */
   for (;;) {
      c = *s++;
      spanp = delim;
      do {
         if ((sc = *spanp++) == c) {
            if (c == 0)
               s = NULL;
            else
               s[-1] = 0;
            *lasts = s;
            return tok;
         }
      } while (sc != 0);
   }
}


unsigned int epicsStrHash(const char *str, unsigned int seed)
{
    unsigned int hash = seed;
    char c;

    while ((c = *str++)) {
        hash ^= ~((hash << 11) ^ c ^ (hash >> 5));
        if (!(c = *str++)) break;
        hash ^= (hash << 7) ^ c ^ (hash >> 3);
    }
    return hash;
}

unsigned int epicsMemHash(const char *str, size_t length, unsigned int seed)
{
    unsigned int hash = seed;

    while (length--) {
        hash ^= ~((hash << 11) ^ *str++ ^ (hash >> 5));
        if (!length--) break;
        hash ^= (hash << 7) ^ *str++ ^ (hash >> 3);
    }
    return hash;
}

/* Compute normalized Levenshtein distance
 *
 * https://en.wikipedia.org/wiki/Levenshtein_distance
 *
 * We modify this to give half weight to case insensitive substitution.
 * All normal integer weights are multiplied by two, with case
 * insensitive added in as one.
 */
double epicsStrSimilarity(const char *A, const char *B)
{
    double ret = 0;
    size_t lA, lB, a, b;
    size_t norm;
    size_t *dist0, *dist1, *stemp;

    lA = strlen(A);
    lB = strlen(B);

    /* max number of edits to change A into B is max(lA, lB) */
    norm = lA > lB ? lA : lB;
    /* take into account our weighting */
    norm *= 2u;

    dist0 = calloc(1+lB, sizeof(*dist0));
    dist1 = calloc(1+lB, sizeof(*dist1));
    if(!dist0 || !dist1) {
        ret = -1.0;
        goto done;
    }

    for(b=0; b<1+lB; b++)
        dist0[b] = 2*b;

    for(a=0; a<lA; a++) {
        dist1[0] = 2*(a+1);

        for(b=0; b<lB; b++) {
            size_t delcost = dist0[b+1] + 2,
                   inscost = dist1[b] + 2,
                   subcost = dist0[b],
                   mincost = delcost;
            char ca = A[a], cb = B[b];

            if(ca!=cb)
                subcost++;
            if(toupper((int)ca)!=toupper((int)cb))
                subcost++;

            if(mincost > inscost)
                mincost = inscost;
            if(mincost > subcost)
                mincost = subcost;

            dist1[b+1] = mincost;
        }

        stemp = dist0;
        dist0 = dist1;
        dist1 = stemp;
    }

    ret = norm ? (norm - dist0[lB]) / (double)norm : 1.0;
done:
    free(dist0);
    free(dist1);
    return ret;
}
