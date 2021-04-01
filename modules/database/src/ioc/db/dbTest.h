/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INC_dbTest_H
#define INC_dbTest_H

#include "dbCoreAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

/*dbAddr info */
DBCORE_API long dba(const char *pname);
/*list records*/
DBCORE_API long dbl(
    const char *precordTypename,const char *fields);
/*list number of records of each type*/
DBCORE_API long dbnr(int verbose);
/* list aliases */
DBCORE_API long dbla(const char *pmask);
/* list infos */
DBCORE_API long dbli(const char *patern);
/*list records with mask*/
DBCORE_API long dbgrep(const char *pmask);
/*get field value*/
DBCORE_API long dbgf(const char *pname);
/*put field value*/
DBCORE_API long dbpf(const char *pname,const char *pvalue);
/*print record*/
DBCORE_API long dbpr(const char *pname,int interest_level);
/*test record*/
DBCORE_API long dbtr(const char *pname);
/*test get field*/
DBCORE_API long dbtgf(const char *pname);
/*test put field*/
DBCORE_API long dbtpf(const char *pname,const char *pvalue);
/*I/O report */
DBCORE_API long dbior(
    const char *pdrvName,int interest_level);
/*Hardware Configuration Report*/
DBCORE_API int dbhcr(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_dbTest_H */
