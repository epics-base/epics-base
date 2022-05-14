/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* Author:  Marty Kraimer Date:    02-23-94*/

#ifndef INCdbAsLibh
#define INCdbAsLibh

#include <stdio.h>

#include "callback.h"
#include "dbCoreAPI.h"

typedef struct {
    epicsCallback   callback;
    long            status;
} ASDBCALLBACK;

struct dbChannel;

#ifdef __cplusplus
extern "C" {
#endif

DBCORE_API int asSetFilename(const char *acf);
DBCORE_API int asSetSubstitutions(const char *substitutions);
DBCORE_API int asInit(void);
DBCORE_API int asInitAsyn(ASDBCALLBACK *pcallback);
DBCORE_API int asShutdown(void);
DBCORE_API int asDbGetAsl(struct dbChannel *chan);
DBCORE_API void * asDbGetMemberPvt(struct dbChannel *chan);
DBCORE_API int asdbdump(void);
DBCORE_API int asdbdumpFP(FILE *fp);
DBCORE_API int aspuag(const char *uagname);
DBCORE_API int aspuagFP(FILE *fp,const char *uagname);
DBCORE_API int asphag(const char *hagname);
DBCORE_API int asphagFP(FILE *fp,const char *hagname);
DBCORE_API int asprules(const char *asgname);
DBCORE_API int asprulesFP(FILE *fp,const char *asgname);
DBCORE_API int aspmem(const char *asgname,int clients);
DBCORE_API int aspmemFP(
    FILE *fp,const char *asgname,int clients);
DBCORE_API int astac(
    const char *recordname,const char *user,const char *location);

#ifdef __cplusplus
}
#endif

#endif /*INCdbAsLibh*/
