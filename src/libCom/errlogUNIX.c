/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$
 * errPrintfUNIX.c
 *      Author:          Marty Kraimer
 *      Date:            02-16-95
 ***************************************************************************
 * This must ultimately be replaced by a facility that allows remote
 * nodes access to the error messages. A message handling communication
 * task should be written that allows multiple remote nodes to request
 * notification of all error messages.
 * For now lets just call fprintf and vfprintf
 ***************************************************************************
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define ERRLOG_INIT
#define epicsExportSharedSymbols
#include "epicsAssert.h"
#include "errMdef.h"
#include "error.h"
#include "errlog.h"

static int sevToLog=0;

epicsShareFunc int epicsShareAPIV errlogPrintf(const char *pformat, ...)
{
    va_list	pvar;
    int		nchar;

    va_start(pvar,pformat);
    nchar = errlogVprintf(pformat,pvar);
    va_end (pvar);
    return(nchar);
}

epicsShareFunc int epicsShareAPIV errlogVprintf(
    const char *pformat, va_list pvar)
{
    return(vfprintf(stderr,pformat,pvar));
}

epicsShareFunc int epicsShareAPI errlogMessage(const char *message)
{
    fprintf(stderr,"%s\n",message);
    return(0);
}

epicsShareFunc int epicsShareAPIV errlogSevPrintf(
    const errlogSevEnum severity,const char *pformat, ...)
{
    va_list     pvar;
    int		nchar;
 
    if(sevToLog>severity) return(0);
    va_start(pvar, pformat);
    nchar = errlogSevVprintf(severity,pformat,pvar);
    va_end (pvar);
    return(nchar);
}

epicsShareFunc int epicsShareAPIV errlogSevVprintf(
    const errlogSevEnum severity,const char *pformat,va_list pvar)
{
    int         nchar;
 
    if(sevToLog>severity) return(0);
    fprintf(stderr,"sevr=%s ",errlogGetSevEnumString(severity));
    nchar = vfprintf(stderr,pformat,pvar);
    fprintf(stderr,"\n");
    return(nchar+1);
}

epicsShareFunc char * epicsShareAPI errlogGetSevEnumString(
    const errlogSevEnum severity)
{
    static char unknown[] = "unknown";
 
    if(severity<0 || severity>3) return(unknown);
    return(errlogSevEnumString[severity]);
}


epicsShareFunc void epicsShareAPI errlogSetSevToLog(
    const errlogSevEnum severity )
{
    sevToLog = severity;
}
 
epicsShareFunc errlogSevEnum epicsShareAPI errlogGetSevToLog()
{
    return(sevToLog);
}

epicsShareFunc void epicsShareAPI errlogAddListener(
    errlogListener listener)
{
    fprintf(stderr,"errlogAddListener not implemented\n");
    return;
}

epicsShareFunc void epicsShareAPI errlogRemoveListener(
    errlogListener listener)
{
    fprintf(stderr,"errlogRemoveListener not implemented\n");
    return;
}


epicsShareFunc void epicsShareAPIV errPrintf(long status, const char *pFileName, 
	int lineno, const char *pformat, ...)
{
    va_list 	   pvar;

    va_start(pvar, pformat);
    if(pFileName && errVerbose){
      	fprintf(stderr,"filename=\"%s\" line number=%d\n", pFileName, lineno);
    }
    if(status==0) status = errno;
    if(status>0) {
	int rtnval;
	char	name[256];

	rtnval = errSymFind(status,name);
	if(rtnval) {
	    unsigned short modnum,errnum;

	    modnum = (unsigned short) (status >> 16);
	    errnum = (unsigned short) (status & 0xffff);
	    fprintf(stderr, "status (%hu,%hu) not in symbol table",
		modnum, errnum);
	} else {
	    fprintf(stderr,"%s ",name);
	}
    }
    if (pformat) {
    	vfprintf(stderr,pformat,pvar);
    }
    fprintf(stderr,"\n");
}
