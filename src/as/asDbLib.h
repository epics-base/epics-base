/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* share/epicsH/dbAsLib.h	*/
/*  $Id$ */
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

epicsShareFunc int epicsShareAPI asSetFilename(char *acf);
epicsShareFunc int epicsShareAPI asSetSubstitutions(char *substitutions);
epicsShareFunc int epicsShareAPI asInit(void);
epicsShareFunc int epicsShareAPI asInitAsyn(ASDBCALLBACK *pcallback);
epicsShareFunc int epicsShareAPI asDbGetAsl( void *paddr);
epicsShareFunc void *  epicsShareAPI asDbGetMemberPvt( void *paddr);
epicsShareFunc int epicsShareAPI asdbdump(void);
epicsShareFunc int epicsShareAPI asdbdumpFP(FILE *fp);
epicsShareFunc int epicsShareAPI aspuag(char *uagname);
epicsShareFunc int epicsShareAPI aspuagFP(FILE *fp,char *uagname);
epicsShareFunc int epicsShareAPI asphag(char *hagname);
epicsShareFunc int epicsShareAPI asphagFP(FILE *fp,char *hagname);
epicsShareFunc int epicsShareAPI asprules(char *asgname);
epicsShareFunc int epicsShareAPI asprulesFP(FILE *fp,char *asgname);
epicsShareFunc int epicsShareAPI aspmem(char *asgname,int clients);
epicsShareFunc int epicsShareAPI aspmemFP(FILE *fp,char *asgname,int clients);
epicsShareFunc int epicsShareAPI astac(
    char *recordname,char *user,char *location);

#ifdef __cplusplus
}
#endif

#endif /*INCdbAsLibh*/
