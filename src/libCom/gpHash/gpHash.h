/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* gpHash.h */
/* share/epicsH $Id$ */
/* Author:  Marty Kraimer Date:    04-07-94 */

/* gph provides a general purpose directory accessed via a hash table*/
#ifndef INCgpHashh
#define INCgpHashh 1

#include "shareLib.h"

#include "ellLib.h"

typedef struct{
    ELLNODE	node;
    const char	*name;		/*address of name placed in directory*/
    void	*pvtid;		/*private name for subsystem user*/
    void	*userPvt;	/*private for user*/
} GPHENTRY;

#ifdef __cplusplus
extern "C" {
#endif

/*tableSize must be power of 2 in range 256 to 65536*/
epicsShareFunc void epicsShareAPI gphInitPvt(void **ppvt,int tableSize);
epicsShareFunc GPHENTRY * epicsShareAPI
    gphFind(void *pvt,const char *name,void *pvtid);
epicsShareFunc GPHENTRY * epicsShareAPI
    gphAdd(void *pvt,const char *name,void *pvtid);
epicsShareFunc void epicsShareAPI
    gphDelete(void *pvt,const char *name,void *pvtid);
epicsShareFunc void epicsShareAPI gphFreeMem(void *pvt);
epicsShareFunc void epicsShareAPI gphDump(void *pvt);
epicsShareFunc void epicsShareAPI gphDumpFP(FILE *fp,void *pvt);

#ifdef __cplusplus
}
#endif

#endif /*INCgpHashh*/
