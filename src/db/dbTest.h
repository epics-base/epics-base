/* dbTest.h */

/* Author:  Marty Kraimer Date:    25FEB2000 */
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO

This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/
#ifndef INCdbTesth
#define INCdbTesth 1

#include "shareLib.h"

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
epicsShareFunc long dbior(char	*pdrvName,int type);
/*Hardware Configuration Report*/
epicsShareFunc int dbhcr(char *filename);

#endif /*INCdbTesth */
