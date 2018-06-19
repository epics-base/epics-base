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

#define epicsExportSharedSymbols
#include "epicsStdio.h"
#include "cantProceed.h"
#include "epicsString.h"

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

        case '0' :case '1' :case '2' :case '3' :
        case '4' :case '5' :case '6' :case '7' :
            { /* \ooo */
                unsigned int u = c - '0';

                if (!srclen-- || !(c = *src++)) {
                    OUT(u); goto done;
                }
                if (c < '0' || c > '7') {
                    OUT(u); goto input;
                }
                u = u << 3 | (c - '0');

                if (!srclen-- || !(c = *src++)) {
                    OUT(u); goto done;
                }
                if (c < '0' || c > '7') {
                    OUT(u); goto input;
                }
                u = u << 3 | (c - '0');

                if (u > 0377) {
                    /* Undefined behaviour! */
                }
                OUT(u);
            }
            break;

        case 'x' :
            { /* \xXXX... */
                unsigned int u = 0;

                if (!srclen-- || !(c = *src++ & 0xff))
                    goto done;

                while (isxdigit(c)) {
                    u = u << 4 | ((c > '9') ? toupper(c) - 'A' + 10 : c - '0');
                    if (u > 0xff) {
                        /* Undefined behaviour! */
                    }
                    if (!srclen-- || !(c = *src++ & 0xff)) {
                        OUT(u);
                        goto done;
                    }
                }
                OUT(u);
                goto input;
            }

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
        default:
            if (isprint(c & 0xff)) {
                OUT(c);
                break;
            }
            OUT('\\');
            OUT('0' + ((c & 0300) >> 6));
            OUT('0' + ((c & 0070) >> 3));
            OUT('0' +  (c & 0007));
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
        case '\'': case '\"':
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

   while (len--) {
       char c = *s++;

       switch (c) {
       case '\a':  nout += fprintf(fp, "\\a");  break;
       case '\b':  nout += fprintf(fp, "\\b");  break;
       case '\f':  nout += fprintf(fp, "\\f");  break;
       case '\n':  nout += fprintf(fp, "\\n");  break;
       case '\r':  nout += fprintf(fp, "\\r");  break;
       case '\t':  nout += fprintf(fp, "\\t");  break;
       case '\v':  nout += fprintf(fp, "\\v");  break;
       case '\\':  nout += fprintf(fp, "\\\\"); break;
       case '\'':  nout += fprintf(fp, "\\'");  break;
       case '\"':  nout += fprintf(fp, "\\\"");  break;
       default:
           if (isprint(0xff & (int)c))
               nout += fprintf(fp, "%c", c);
           else
               nout += fprintf(fp, "\\%03o", (unsigned char)c);
           break;
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

int epicsStrGlobMatch(const char *str, const char *pattern)
{
    const char *cp = NULL, *mp = NULL;

    while ((*str) && (*pattern != '*')) {
        if ((*pattern != *str) && (*pattern != '?'))
            return 0;
        pattern++;
        str++;
    }
    while (*str) {
        if (*pattern == '*') {
            if (!*++pattern)
                return 1;
            mp = pattern;
            cp = str+1;
        }
        else if ((*pattern == *str) || (*pattern == '?')) {
            pattern++;
            str++;
        }
        else {
            pattern = mp;
            str = cp++;
        }
    }
    while (*pattern == '*')
        pattern++;
    return !*pattern;
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
