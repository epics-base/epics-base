/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* dbCaTest.h */

/* Author:  Marty Kraimer Date:    25FEB2000 */
#ifndef INCdbTesth
#define INCdbTesth 1

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

epicsShareFunc long epicsShareAPI dbcar(char *recordname,int level);

#ifdef __cplusplus
}
#endif

#endif /*INCdbTesth */
