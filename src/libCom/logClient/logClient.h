/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/* $Id$ */
/*
 *
 *      Author:         Jeffrey O. Hill 
 *      Date:           080791 
 */

#ifndef INClogClienth
#define INClogClienth 1
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *logClientId;
epicsShareFunc logClientId epicsShareAPI logClientInit (void);
epicsShareFunc void epicsShareAPI logClientSendMessage (logClientId id, const char *message);
epicsShareFunc void epicsShareAPI logClientShow (logClientId id, unsigned level);
epicsShareFunc void epicsShareAPI logClientFlush (logClientId id);

/*
 * default log client interface
 */
epicsShareExtern int iocLogDisable;
epicsShareFunc int epicsShareAPI iocLogInit (void);
epicsShareFunc void epicsShareAPI iocLogShow (unsigned level);
epicsShareFunc void epicsShareAPI iocLogFlush ();

#ifdef __cplusplus
}
#endif

#endif /*INClogClienth*/
