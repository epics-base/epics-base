/*************************************************************************\
* Copyright (c) 2007 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INCLdb_testh
#define INCLdb_testh

#include "dbCoreAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

DBCORE_API int gft(const char *pname);
DBCORE_API int pft(const char *pname, const char *pvalue);
DBCORE_API int tpn(const char *pname, const char *pvalue);
#ifdef __cplusplus
}
#endif

#endif /* INCLdb_testh */
