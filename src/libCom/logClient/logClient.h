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

#ifndef INClogClienth
#define INClogClienth 1
#include "shareLib.h"
#include "osiSock.h" /* for 'struct in_addr' */

/* include default log client interface for backward compatibility */
#include "iocLog.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *logClientId;
epicsShareFunc logClientId epicsShareAPI logClientCreate (
    struct in_addr server_addr, unsigned short server_port);
epicsShareFunc void epicsShareAPI logClientSend (logClientId id, const char *message);
epicsShareFunc void epicsShareAPI logClientShow (logClientId id, unsigned level);
epicsShareFunc void epicsShareAPI logClientFlush (logClientId id);

/* deprecated interface; retained for backward compatibility */
/* note: implementations are in iocLog.c, not logClient.c */
epicsShareFunc logClientId epicsShareAPI logClientInit (void);
epicsShareFunc void logClientSendMessage (logClientId id, const char *message);

#ifdef __cplusplus
}
#endif

#endif /*INClogClienth*/
