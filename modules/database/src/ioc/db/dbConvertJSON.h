/*************************************************************************\
* Copyright (c) 2016 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbConvertJSON.h */

#ifndef INC_dbConvertJSON_H
#define INC_dbConvertJSON_H

#include <shareLib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* This name should probably be changed to inclue "array" */
epicsShareFunc long dbPutConvertJSON(const char *json, short dbrType,
    void *pdest, long *psize);
epicsShareFunc long dbLSConvertJSON(const char *json, char *pdest,
    epicsUInt32 size, epicsUInt32 *plen);
#ifdef __cplusplus
}
#endif

#endif /* INC_dbConvertJSON_H */

