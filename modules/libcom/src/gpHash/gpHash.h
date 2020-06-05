/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* Author:  Marty Kraimer Date:    04-07-94 */

/* gph provides a general purpose directory accessed via a hash table*/

#ifndef INC_gpHash_H
#define INC_gpHash_H

#include "libComAPI.h"

#include "ellLib.h"

typedef struct{
    ELLNODE     node;
    const char  *name;          /*address of name placed in directory*/
    void        *pvtid;         /*private name for subsystem user*/
    void        *userPvt;       /*private for user*/
} GPHENTRY;

struct gphPvt;

#ifdef __cplusplus
extern "C" {
#endif

/*tableSize must be power of 2 in range 256 to 65536*/
LIBCOM_API void epicsStdCall
    gphInitPvt(struct gphPvt **ppvt, int tableSize);
LIBCOM_API GPHENTRY * epicsStdCall
    gphFind(struct gphPvt *pvt, const char *name, void *pvtid);
LIBCOM_API GPHENTRY * epicsStdCall
    gphFindParse(struct gphPvt *pvt, const char *name, size_t len, void *pvtid);
LIBCOM_API GPHENTRY * epicsStdCall
    gphAdd(struct gphPvt *pvt, const char *name, void *pvtid);
LIBCOM_API void epicsStdCall
    gphDelete(struct gphPvt *pvt, const char *name, void *pvtid);
LIBCOM_API void epicsStdCall gphFreeMem(struct gphPvt *pvt);
LIBCOM_API void epicsStdCall gphDump(struct gphPvt *pvt);
LIBCOM_API void epicsStdCall gphDumpFP(FILE *fp, struct gphPvt *pvt);

#ifdef __cplusplus
}
#endif

#endif /* INC_gpHash_H */
