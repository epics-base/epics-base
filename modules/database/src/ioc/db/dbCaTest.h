/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INC_dbCaTest_H
#define INC_dbCaTest_H

#include "dbCoreAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

DBCORE_API long dbcar(char *recordname,int level);
DBCORE_API void dbcaStats(int *pchans, int *pdiscon);

#ifdef __cplusplus
}
#endif

#endif /* INC_dbCaTest_H */
