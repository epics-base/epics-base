/* $Id$
 * errPrintfUNIX.c
 *      Author:          Marty Kraimer
 *      Date:            02-16-95
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 * Modification Log: errPrintfUNIX.c
 * -----------------
 * .01  02-16-95	mrk	Extracted from errSymLib.c
 ***************************************************************************
 * This must ultimately be replaced by a facility that allows remote
 * nodes access to the error messages. A message handling communication
 * task should be written that allows multiple remote nodes to request
 * notification of all error messages.
 * For now lets just print messages and last errno via logMsg or printf
 ***************************************************************************
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <epicsAssert.h>
#include <dbDefs.h>
#include <errMdef.h>
#include <epicsPrint.h>
#include "errSymTbl.h"


/****************************************************************
 * ERRMESSAGE - now a macro to call errPrintf
 * ERRPRINTF  - print an error symbol message
 ***************************************************************/
void errPrintf(long status, const char *pFileName, 
	int lineno, const char *pformat, ...)
{
    va_list 	   pvar;

    va_start(pvar, pformat);
    if(pFileName && errVerbose){
      	printf("filename=\"%s\" line number=%d\n", pFileName, lineno);
    }
    if(status==0) status = errno;
    if(status>0) {
	int rtnval;
	unsigned short modnum,errnum;
	char	name[256];

	rtnval = errSymFind(status,name);
	modnum = status >> 16;
	errnum = status & 0xffff;
	if(rtnval) {
	    printf( "Error status (module %hu, number %hu) not in symbol table",
		modnum, errnum);
	} else {
	    printf("%s ",name);
	}
    }
    vprintf(pformat,pvar);
    printf("\n");
}

