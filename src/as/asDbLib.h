/* share/epicsH/dbAsLib.h	*/
/*  $Id$ */
/* Author:  Marty Kraimer Date:    02-23-94*/
/*****************************************************************
                          COPYRIGHT NOTIFICATION
*****************************************************************

(C)  COPYRIGHT 1993 UNIVERSITY OF CHICAGO

This software was developed under a United States Government license
described on the COPYRIGHT_UniversityOfChicago file included as part
of this distribution.
**********************************************************************/

/*
 * Modification Log:
 * -----------------
 * .01  02-23-94	mrk	Initial Implementation
 */

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
epicsShareFunc int epicsShareAPI asdbdump( void);
epicsShareFunc int epicsShareAPI aspuag(char *uagname);
epicsShareFunc int epicsShareAPI asphag(char *hagname);
epicsShareFunc int epicsShareAPI asprules(char *asgname);
epicsShareFunc int epicsShareAPI aspmem(char *asgname,int clients);
epicsShareFunc int epicsShareAPI astac(
    char *recordname,char *user,char *location);

#ifdef __cplusplus
}
#endif

#endif /*INCdbAsLibh*/
