/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbTest.h */

/* Author:  Marty Kraimer Date:    25FEB2000 */

#ifndef INCdbTesth
#define INCdbTesth 1

#include "shareLib.h"

#include "shareLib.h"
#ifdef __cplusplus
extern "C" {
#endif

/*dbAddr info */
epicsShareFunc long epicsShareAPI dba(char*pname);
/*list records*/
epicsShareFunc long epicsShareAPI dbl(char *precordTypename,char *filename,char *fields);
/*list number of records of each type*/
epicsShareFunc long epicsShareAPI dbnr(int verbose);
/*list records with mask*/
epicsShareFunc long epicsShareAPI dbgrep(char *pmask);
/*get field value*/
epicsShareFunc long epicsShareAPI dbgf(char	*pname);
/*put field value*/
epicsShareFunc long epicsShareAPI dbpf(char	*pname,char *pvalue);
/*print record*/
epicsShareFunc long epicsShareAPI dbpr(char *pname,int interest_level);
/*test record*/
epicsShareFunc long epicsShareAPI dbtr(char *pname);
/*test get field*/
epicsShareFunc long epicsShareAPI dbtgf(char *pname);
/*test put field*/
epicsShareFunc long epicsShareAPI dbtpf(char	*pname,char *pvalue);
/*I/O report */
epicsShareFunc long epicsShareAPI dbior(char	*pdrvName,int interest_level);
/*Hardware Configuration Report*/
epicsShareFunc int epicsShareAPI dbhcr(char *filename);

#ifdef __cplusplus
}
#endif

#endif /*INCdbTesth */
