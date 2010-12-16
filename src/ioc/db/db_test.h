/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef INCLdb_testh
#define INCLdb_testh

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc int epicsShareAPI gft(char *pname);
epicsShareFunc int epicsShareAPI pft(char *pname,char *pvalue);
epicsShareFunc int epicsShareAPI tpn(char     *pname,char *pvalue);
#ifdef __cplusplus
}
#endif

#endif /* INCLdb_testh */
