/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* epicsStdioTest.c
 *
 *      Author  Marty Kraimer
 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#include "epicsStdio.h"

static int xsnprintf(char *str, size_t size, const char *format, ...)
{
    int nchar;
    va_list     pvar;

    va_start(pvar,format);
    nchar = epicsVsnprintf(str,size,format,pvar);
    va_end (pvar);
    return(nchar);
}

    
int epicsStdioTest ()
{
    char buffer[20];
    int rtn;
    int value = 10;
    int size = 10;

    memset(buffer,'*',sizeof(buffer)-1);
    buffer[sizeof(buffer)-1] = 0;
    printf("at start buffer |%s|\n",buffer);
    rtn = xsnprintf(buffer,size,"value is  %d",value);
    printf("size %d rtn %d value %d buffer |%s|\n",size,rtn,value,buffer);
    memset(buffer,'*',sizeof(buffer)-1);
    rtn = xsnprintf(buffer,size,"value:  %d",value);
    printf("size %d rtn %d value %d buffer |%s|\n",size,rtn,value,buffer);
    memset(buffer,'*',sizeof(buffer)-1);
    rtn = xsnprintf(buffer,size,"%d",value);
    printf("size %d rtn %d value %d buffer |%s|\n",size,rtn,value,buffer);

    memset(buffer,'*',sizeof(buffer)-1);
    buffer[sizeof(buffer)-1] = 0;
    printf("at start buffer |%s|\n",buffer);
    rtn = epicsSnprintf(buffer,size,"value is  %d",value);
    printf("size %d rtn %d value %d buffer |%s|\n",size,rtn,value,buffer);
    memset(buffer,'*',sizeof(buffer)-1);
    rtn = epicsSnprintf(buffer,size,"value:  %d",value);
    printf("size %d rtn %d value %d buffer |%s|\n",size,rtn,value,buffer);
    memset(buffer,'*',sizeof(buffer)-1);
    rtn = epicsSnprintf(buffer,size,"%d",value);
    printf("size %d rtn %d value %d buffer |%s|\n",size,rtn,value,buffer);
    return(0);
}
