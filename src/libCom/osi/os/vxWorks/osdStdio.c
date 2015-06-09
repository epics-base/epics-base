/* osdStdio.c */
/*************************************************************************\
* Copyright (c) 2013 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string.h>
#include <vxWorks.h>
#include <fioLib.h>
#include "epicsStdio.h"
#include "dbDefs.h"

struct outStr_s {
    char *str;
    size_t free;
};

static STATUS outRoutine(char *buffer, size_t nchars, int outarg) {
    struct outStr_s *poutStr = (struct outStr_s *) outarg;
    size_t free = poutStr->free;
    size_t len;
    
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
    size_t nchars;
    va_list pvar;

    va_start(pvar,format);
    nchars = epicsVsnprintf(str,size,format,pvar);
    va_end (pvar);
    return(nchars);
}
