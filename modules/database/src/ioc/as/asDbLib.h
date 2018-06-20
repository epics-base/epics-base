/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* Author:  Marty Kraimer Date:    02-23-94*/

#ifndef INCdbAsLibh
#define INCdbAsLibh

#include "callback.h"
#include "shareLib.h"

typedef struct {
    CALLBACK	callback;
    long	status;
} ASDBCALLBACK;

struct dbChannel;

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc int asSetFilename(const char *acf);
epicsShareFunc int asSetSubstitutions(const char *substitutions);
epicsShareFunc int asInit(void);
epicsShareFunc int asInitAsyn(ASDBCALLBACK *pcallback);
epicsShareFunc int asShutdown(void);
epicsShareFunc int asDbGetAsl(struct dbChannel *chan);
epicsShareFunc void * asDbGetMemberPvt(struct dbChannel *chan);
epicsShareFunc int asdbdump(void);
epicsShareFunc int asdbdumpFP(FILE *fp);
epicsShareFunc int aspuag(const char *uagname);
epicsShareFunc int aspuagFP(FILE *fp,const char *uagname);
epicsShareFunc int asphag(const char *hagname);
epicsShareFunc int asphagFP(FILE *fp,const char *hagname);
epicsShareFunc int asprules(const char *asgname);
epicsShareFunc int asprulesFP(FILE *fp,const char *asgname);
epicsShareFunc int aspmem(const char *asgname,int clients);
epicsShareFunc int aspmemFP(
    FILE *fp,const char *asgname,int clients);
epicsShareFunc int astac(
    const char *recordname,const char *user,const char *location);

#ifdef __cplusplus
}
#endif

#endif /*INCdbAsLibh*/
