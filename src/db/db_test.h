/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* base/include/db_test.h */

#ifndef INCLdb_testh
#define INCLdb_testh

#ifdef __cplusplus
extern "C" {
#endif

#include "shareLib.h"

epicsShareFunc int epicsShareAPI gft(char *pname);
epicsShareFunc int epicsShareAPI pft(char *pname,char *pvalue);
epicsShareFunc int epicsShareAPI tpn(char     *pname,char *pvalue);
#ifdef __cplusplus
}
#endif

#endif /* INCLdb_testh */
