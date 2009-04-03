/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* logClient.h,v 1.5.2.1 2003/07/08 00:08:06 jhill Exp */
/*
 *
 *      Author:         Jeffrey O. Hill 
 *      Date:           080791 
 */

#ifndef INCiocLogh
#define INCiocLogh 1
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ioc log client interface
 */
epicsShareExtern int iocLogDisable;
epicsShareFunc int epicsShareAPI iocLogInit (void);
epicsShareFunc void epicsShareAPI iocLogShow (unsigned level);
epicsShareFunc void epicsShareAPI iocLogFlush (void);

#ifdef __cplusplus
}
#endif

#endif /*INCiocLogh*/
