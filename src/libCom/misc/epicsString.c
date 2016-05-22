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
int dbTranslateEscape(char *to, const char *from)
{
    size_t big_enough = strlen(from)+1;
    return epicsStrnRawFromEscaped(to, big_enough, from, big_enough);
}

int epicsStrnRawFromEscaped(char *to, size_t outsize, const char *from,
    size_t inlen)
{
    const char *pfrom  = from;
    char       *pto = to;
    char        c;
    int         nto = 0, nfrom = 0;

    if (outsize == 0)
        return 0;

    while ((c = *pfrom++) && nto < outsize && nfrom < inlen) {
        nfrom++;
        if (c == '\\') {
            if (nfrom >= inlen || *pfrom == '\0') break;
            switch (*pfrom) {
            case 'a':  pfrom++; nfrom++; *pto++ = '\a' ; nto++; break;
            case 'b':  pfrom++; nfrom++; *pto++ = '\b' ; nto++; break;
            case 'f':  pfrom++; nfrom++; *pto++ = '\f' ; nto++; break;
            case 'n':  pfrom++; nfrom++; *pto++ = '\n' ; nto++; break;
            case 'r':  pfrom++; nfrom++; *pto++ = '\r' ; nto++; break;
            case 't':  pfrom++; nfrom++; *pto++ = '\t' ; nto++; break;
            case 'v':  pfrom++; nfrom++; *pto++ = '\v' ; nto++; break;
            case '\\': pfrom++; nfrom++; *pto++ = '\\' ; nto++; break;
            case '\?': pfrom++; nfrom++; *pto++ = '\?' ; nto++; break;
            case '\'': pfrom++; nfrom++; *pto++ = '\'' ; nto++; break;
            case '\"': pfrom++; nfrom++; *pto++ = '\"' ; nto++; break;
            case '0' :case '1' :case '2' :case '3' :
            case '4' :case '5' :case '6' :case '7' :
                {
                    int  i;
                    char strval[4] = {0,0,0,0};
                    unsigned int  ival;
                    unsigned char *pchar;

                    for (i=0; i<3; i++) {
                        if ((*pfrom < '0') || (*pfrom > '7')) break;
                        strval[i] = *pfrom++; nfrom++;
                    }
                    sscanf(strval,"%o",&ival);
                    pchar = (unsigned char *)(pto++); nto++;
                    *pchar = (unsigned char)(ival);
                }
                break;
            case 'x' :
                {
                    int  i;
                    char strval[3] = {0,0,0};
                    unsigned int  ival;
                    unsigned char *pchar;

                    pfrom++; /*skip the x*/
                    for (i=0; i<2; i++) {
                        if (!isxdigit(0xff & (int)*pfrom)) break;
                        strval[i] = *pfrom++; nfrom++;
                    }
                    sscanf(strval,"%x",&ival);
                    pchar = (unsigned char *)(pto++); nto++;
                    *pchar = (unsigned char)(ival);
                }
                break;
            default:
                *pto++ = *pfrom++; nfrom++; nto++;
            }
        } else {
            *pto++ = c; nto++;
        }
    }
    if (nto == outsize)
        pto--;
    *pto = '\0';
    return nto;
}

int epicsStrnEscapedFromRaw(char *outbuf, size_t outsize, const char *inbuf,
    size_t inlen)
{
    int maxout = outsize;
    int nout = 0;
    int len;
    char *outpos = outbuf;

    while (inlen--)  {
        char c = *inbuf++;
        switch (c) {
            case '\a':  len = epicsSnprintf(outpos, maxout, "\\a"); break;
            case '\b':  len = epicsSnprintf(outpos, maxout, "\\b"); break;
            case '\f':  len = epicsSnprintf(outpos, maxout, "\\f"); break;
            case '\n':  len = epicsSnprintf(outpos, maxout, "\\n"); break;
            case '\r':  len = epicsSnprintf(outpos, maxout, "\\r"); break;
            case '\t':  len = epicsSnprintf(outpos, maxout, "\\t"); break;
            case '\v':  len = epicsSnprintf(outpos, maxout, "\\v"); break;
            case '\\':  len = epicsSnprintf(outpos, maxout, "\\\\"); ; break;
            case '\'':  len = epicsSnprintf(outpos, maxout, "\\'"); break;
            case '\"':  len = epicsSnprintf(outpos, maxout, "\\\""); break;
            default:
                if (isprint(0xff & (int)c))
                    len = epicsSnprintf(outpos, maxout, "%c", c);
                else
                    len = epicsSnprintf(outpos, maxout, "\\%03o",
                        (unsigned char)c);
                break;
        }
        if (len<0) return -1;
        nout += len;
        if (nout < outsize) {
            maxout -= len;
            outpos += len;
        } else {
            outpos = outpos + maxout -1;
            maxout = 1;
        }
    }
    *outpos = '\0';
    return nout;
}

size_t epicsStrnEscapedFromRawSize(const char *inbuf, size_t inlen)
{
    size_t nout = inlen;

    while (inlen--) {
        char c = *inbuf++;

        switch (c) {
        case '\a': case '\b': case '\f': case '\n':
        case '\r': case '\t': case '\v': case '\\':
        case '\'': case '\"':
            nout++;
            break;
        default:
            if (!isprint(0xff & (int)c))
                nout += 3;
        }
    }
    return nout;
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
