/*************************************************************************\
* Copyright (c) 2016 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* dbConvertJSON.h */

#ifndef INC_dbConvertJSON_H
#define INC_dbConvertJSON_H

#include <dbCoreAPI.h>

#ifdef __cplusplus
extern "C" {
#endif

/* This name should probably be changed to include "array" */
DBCORE_API long dbPutConvertJSON(const char *json, short dbrType,
    void *pdest, long *psize);
DBCORE_API long dbLSConvertJSON(const char *json, char *pdest,
    epicsUInt32 size, epicsUInt32 *plen);
#ifdef __cplusplus
}
#endif

#endif /* INC_dbConvertJSON_H */

