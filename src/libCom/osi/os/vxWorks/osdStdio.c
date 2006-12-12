/* osdStdio.c */
/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include "epicsStdio.h"
#include "fioLib.h"
#include "string.h"
#include "dbDefs.h"

struct outStr_s {
    char *str;
    int free;
};

static STATUS outRoutine(char *buffer, int nchars, int outarg) {
    struct outStr_s *poutStr = (struct outStr_s *) outarg;
    int free = poutStr->free;
    int len;
    
    if (free < 1) { /*let fioFormatV continue to count length*/
        return OK;
    } else if (free > 1) {
        len = min(free-1, nchars);
        strncpy(poutStr->str, buffer, len);
        poutStr->str += len;
        poutStr->free -= len;
    }
    /*make sure final string is null terminated*/
    *poutStr->str = 0;
    return OK;
}

int epicsVsnprintf(char *str, size_t size, const char *format, va_list ap) {
    struct outStr_s outStr;
    
    outStr.str = str;
    outStr.free = size;
    return fioFormatV(format, ap, outRoutine, (int) &outStr);
}

int epicsSnprintf(char *str, size_t size, const char *format, ...)
{
    int nchars;
    va_list pvar;

    va_start(pvar,format);
    nchars = epicsVsnprintf(str,size,format,pvar);
    va_end (pvar);
    return(nchars);
}
