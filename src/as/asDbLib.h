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

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc int epicsShareAPI asSetFilename(const char *acf);
epicsShareFunc int epicsShareAPI asSetSubstitutions(const char *substitutions);
epicsShareFunc int epicsShareAPI asInit(void);
epicsShareFunc int epicsShareAPI asInitAsyn(ASDBCALLBACK *pcallback);
epicsShareFunc int epicsShareAPI asDbGetAsl( void *paddr);
epicsShareFunc void *  epicsShareAPI asDbGetMemberPvt( void *paddr);
epicsShareFunc int epicsShareAPI asdbdump(void);
epicsShareFunc int epicsShareAPI asdbdumpFP(FILE *fp);
epicsShareFunc int epicsShareAPI aspuag(const char *uagname);
epicsShareFunc int epicsShareAPI aspuagFP(FILE *fp,const char *uagname);
epicsShareFunc int epicsShareAPI asphag(const char *hagname);
epicsShareFunc int epicsShareAPI asphagFP(FILE *fp,const char *hagname);
epicsShareFunc int epicsShareAPI asprules(const char *asgname);
epicsShareFunc int epicsShareAPI asprulesFP(FILE *fp,const char *asgname);
epicsShareFunc int epicsShareAPI aspmem(const char *asgname,int clients);
epicsShareFunc int epicsShareAPI aspmemFP(
    FILE *fp,const char *asgname,int clients);
epicsShareFunc int epicsShareAPI astac(
    const char *recordname,const char *user,const char *location);

#ifdef __cplusplus
}
#endif

#endif /*INCdbAsLibh*/
