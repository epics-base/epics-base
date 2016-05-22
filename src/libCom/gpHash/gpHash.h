/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* Author:  Marty Kraimer Date:    04-07-94 */

/* gph provides a general purpose directory accessed via a hash table*/

#ifndef INC_gpHash_H
#define INC_gpHash_H

#include "shareLib.h"

#include "ellLib.h"

typedef struct{
    ELLNODE	node;
    const char	*name;		/*address of name placed in directory*/
    void	*pvtid;		/*private name for subsystem user*/
    void	*userPvt;	/*private for user*/
} GPHENTRY;

struct gphPvt;

#ifdef __cplusplus
extern "C" {
#endif

/*tableSize must be power of 2 in range 256 to 65536*/
epicsShareFunc void epicsShareAPI
    gphInitPvt(struct gphPvt **ppvt, int tableSize);
epicsShareFunc GPHENTRY * epicsShareAPI
    gphFind(struct gphPvt *pvt, const char *name, void *pvtid);
epicsShareFunc GPHENTRY * epicsShareAPI
    gphFindParse(struct gphPvt *pvt, const char *name, size_t len, void *pvtid);
epicsShareFunc GPHENTRY * epicsShareAPI
    gphAdd(struct gphPvt *pvt, const char *name, void *pvtid);
epicsShareFunc void epicsShareAPI
    gphDelete(struct gphPvt *pvt, const char *name, void *pvtid);
epicsShareFunc void epicsShareAPI gphFreeMem(struct gphPvt *pvt);
epicsShareFunc void epicsShareAPI gphDump(struct gphPvt *pvt);
epicsShareFunc void epicsShareAPI gphDumpFP(FILE *fp, struct gphPvt *pvt);

#ifdef __cplusplus
}
#endif

#endif /* INC_gpHash_H */
