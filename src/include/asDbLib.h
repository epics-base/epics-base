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
#include <asLib.h>
#include <callback.h>

typedef struct {
    CALLBACK	callback;
    long	status;
} ASDBCALLBACK;

int asSetFilename(char *acf);
int asSetSubstitutions(char *substitutions);
int asInit(void);
int asInitAsyn(ASDBCALLBACK *pcallback);
int asDbGetAsl( void *paddr);
ASMEMBERPVT  asDbGetMemberPvt( void *paddr);
int asdbdump( void);
int aspuag(char *uagname);
int asphag(char *hagname);
int asprules(char *asgname);
int aspmem(char *asgname,int clients);
void asCaStart(void);
void asCaStop(void);

#endif /*INCdbAsLibh*/
