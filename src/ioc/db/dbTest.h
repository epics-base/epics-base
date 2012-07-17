/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef INC_dbTest_H
#define INC_dbTest_H

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/*dbAddr info */
epicsShareFunc long dba(const char *pname);
/*list records*/
epicsShareFunc long dbl(
    const char *precordTypename,const char *fields);
/*list number of records of each type*/
epicsShareFunc long dbnr(int verbose);
/* list aliases */
epicsShareFunc long dbla(const char *pmask);
/*list records with mask*/
epicsShareFunc long dbgrep(const char *pmask);
/*get field value*/
epicsShareFunc long dbgf(const char *pname);
/*put field value*/
epicsShareFunc long dbpf(const char *pname,const char *pvalue);
/*print record*/
epicsShareFunc long dbpr(const char *pname,int interest_level);
/*test record*/
epicsShareFunc long dbtr(const char *pname);
/*test get field*/
epicsShareFunc long dbtgf(const char *pname);
/*test put field*/
epicsShareFunc long dbtpf(const char *pname,const char *pvalue);
/*I/O report */
epicsShareFunc long dbior(
    const char *pdrvName,int interest_level);
/*Hardware Configuration Report*/
epicsShareFunc int dbhcr(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_dbTest_H */
