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
#include <fcntl.h>
#include <errno.h>

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

    
int epicsStdioTest (const char *report)
{
    char buffer[20];
    int rtn;
    int value = 10;
    int size = 10;
    FILE *stream = 0;

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
    printf("\nTest epicsSetThreadStdout/epicsGetStdout stdout %p epicsGetStdout %p\n",
        stdout,epicsGetStdout());
    if(report && strlen(report)>0) {
        int fd;

        errno = 0;
        fd = open(report, O_CREAT | O_WRONLY | O_TRUNC, 0644 );
        if(fd<0) {
            fprintf(stderr,"%s could not be created %s\n",
                report,strerror(errno));
        } else {
            stream = fdopen(fd,"w");
            if(!stream) {
                fprintf(stderr,"%s could not be opened for output %s\n",
                    report,strerror(errno));
            } else {
                epicsSetThreadStdout(stream);
                printf("After epicsSetThreadStdout stream %p epicsGetStdout %p\n",
                    stream,epicsGetStdout());
            }
        }
    }
    printf("This is first line of sample report");
    printf("\nThis is second and last line of sample report\n");
    errno = 0;
    if(stream) {
        epicsSetThreadStdout(0);
        if(fclose(stream)) {
            fprintf(stderr,"fclose failed %s\n",strerror(errno));
        }
    } else {
        fflush(stdout);
    }
    printf("at end stdout %p epicsGetStdout %p\n",stdout,epicsGetStdout());
    return(0);
}
