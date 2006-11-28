/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/*epicsString.c*/
/*Authors: Jun-ichi Odagiri, Marty Kraimer, Eric Norum, Mark Rivers*/

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

epicsShareFunc int dbTranslateEscape(char *to, const char *from)
{
    const char *pfrom  = from;
    char       *pto = to;
    char        c;
    int         nto=0;

    while( (c = *pfrom++ ) ){
	if(c=='\\') {
          switch( *pfrom ){
          case 'a':  pfrom++;  *pto++ = '\a' ; nto++; break;
          case 'b':  pfrom++;  *pto++ = '\b' ; nto++; break;
          case 'f':  pfrom++;  *pto++ = '\f' ; nto++; break;
          case 'n':  pfrom++;  *pto++ = '\n' ; nto++; break;
          case 'r':  pfrom++;  *pto++ = '\r' ; nto++; break;
          case 't':  pfrom++;  *pto++ = '\t' ; nto++; break;
          case 'v':  pfrom++;  *pto++ = '\v' ; nto++; break;
          case '\\': pfrom++;  *pto++ = '\\' ; nto++; break;
          case '\?': pfrom++;  *pto++ = '\?' ; nto++; break;
          case '\'': pfrom++;  *pto++ = '\'' ; nto++; break;
          case '\"': pfrom++;  *pto++ = '\"' ; nto++; break;
          case '0' :case '1' :case '2' :case '3' :
          case '4' :case '5' :case '6' :case '7' :
            {
		int  i;
		char strval[4] = {0,0,0,0};
		unsigned int  ival;
		unsigned char *pchar;

		for(i=0; i<3; i++) {
		    if((*pfrom < '0') || (*pfrom > '7')) break;
		    strval[i] = *pfrom++;
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
		for(i=0; i<2; i++) {
		    if(!isxdigit((int)*pfrom)) break;
		    strval[i] = *pfrom++;
		}
		sscanf(strval,"%x",&ival);
		pchar = (unsigned char *)(pto++); nto++;
		*pchar = (unsigned char)(ival);
            }
            break;
          default:
            *pto++ = *pfrom++;
          }
        } else {
	    *pto++ = c; nto++;
        }
    }
    *pto = '\0'; /*NOTE that nto does not have to be incremented*/
    return(nto);
}

epicsShareFunc int epicsStrCaseCmp(
    const char *s1, const char *s2)
{
    int nexts1,nexts2;
    
    while(1) {
        /* vxWorks implementation expands argument more than once!!! */
        nexts1 = toupper(*s1);
        nexts2 = toupper(*s2);
        if(nexts1==0) return( (nexts2==0) ? 0 : 1 );
        if(nexts2==0) return(-1);
        if(nexts1<nexts2) return(-1);
        if(nexts1>nexts2) return(1);
        s1++;
        s2++;
    }
}

epicsShareFunc int epicsStrnCaseCmp(
    const char *s1, const char *s2, int n)
{
    size_t ind = 0;
    int nexts1,nexts2;
    
    while(1) {
        if(ind++ >= (size_t)n) break;
        /* vxWorks implementation expands argument more than once!!! */
        nexts1 = toupper(*s1);
        nexts2 = toupper(*s2);
        if(nexts1==0) return( (nexts2==0) ? 0 : 1 );
        if(nexts2==0) return(-1);
        if(nexts1<nexts2) return(-1);
        if(nexts1>nexts2) return(1);
        s1++;
        s2++;
    }
    return(0);
}

epicsShareFunc char * epicsStrDup(const char *s)
{
    return strcpy(mallocMustSucceed(strlen(s)+1,"epicsStrDup"),s);
}

epicsShareFunc int epicsStrPrintEscaped(
    FILE *fp, const char *s, int n)
{
   int nout=0;
   while (n--) {
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
       /*? does not follow C convention because trigraphs no longer important*/
       case '\?':  nout += fprintf(fp, "?");  break;
       case '\'':  nout += fprintf(fp, "\\'");  break;
       case '\"':  nout += fprintf(fp, "\\\"");  break;
       default:
           if (isprint((int)c))
               nout += fprintf(fp, "%c", c);/* putchar(c) doesn't work on vxWorks */
           else
               nout += fprintf(fp, "\\%03o", (unsigned char)c);
           break;
       }
   }
   return nout;
}

epicsShareFunc int epicsStrSnPrintEscaped(
    char *outbuf, int outsize, const char *inbuf, int inlen)
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
       /*? does not follow C convention because trigraphs no longer important*/
       case '\?':  len = epicsSnprintf(outpos, maxout, "?"); break;
       case '\'':  len = epicsSnprintf(outpos, maxout, "\\'"); break;
       case '\"':  len = epicsSnprintf(outpos, maxout, "\\\""); break;
       default:
           if (isprint((int)c))
               len = epicsSnprintf(outpos, maxout, "%c", c);
           else
               len = epicsSnprintf(outpos, maxout, "\\%03o", (unsigned char)c);
           break;
       }
       if(len<0) return -1;
       nout += len;
       if(nout < outsize) {
           maxout -= len;
           outpos += len;
       } else {
           outpos = outpos + maxout -1;
           maxout = 1;
       }
   }
   return nout;
}

epicsShareFunc int epicsStrGlobMatch(
    const char *str, const char *pattern)
{
    const char *cp=NULL, *mp=NULL;
    
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

epicsShareFunc char * epicsStrtok_r(char *s, const char *delim, char **lasts)
{
   const char *spanp;
   int c, sc;
   char *tok;


   if (s == NULL && (s = *lasts) == NULL)
      return (NULL);

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
      return (NULL);
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
            return (tok);
         }
      } while (sc != 0);
   }
}
