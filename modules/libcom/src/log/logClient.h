/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
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
#include "libComAPI.h"
#include "osiSock.h" /* for 'struct in_addr' */

/* include default log client interface for backward compatibility */
#include "iocLog.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *logClientId;
LIBCOM_API logClientId epicsStdCall logClientCreate (
    struct in_addr server_addr, unsigned short server_port);
LIBCOM_API void epicsStdCall logClientSend (logClientId id, const char *message);
LIBCOM_API void epicsStdCall logClientShow (logClientId id, unsigned level);
LIBCOM_API void epicsStdCall logClientFlush (logClientId id);
LIBCOM_API void epicsStdCall iocLogPrefix(const char* prefix);

/* deprecated interface; retained for backward compatibility */
/* note: implementations are in iocLog.c, not logClient.c */
LIBCOM_API logClientId epicsStdCall logClientInit (void);

#ifdef __cplusplus
}
#endif

#endif /*INClogClienth*/
