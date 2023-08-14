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
/**
 * \file logClient.h
 *
 * \brief Client on the IOC that forwards log messages to the log server
 *
 * Together with the program iocLogServer, a log client provides generic 
 * support for logging text messages from an IOC or other program to the log
 * server host machine.
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

/** \brief Abstract type that represents handle to the log client
 */
typedef void *logClientId;

/** \brief Creates a new log client
 * 
 * Starts a background thread to connect to server and returns immediately. 
 * If a connection cannot be established, an error message is 
 * printed on the console, but the log client will keep trying to connect in 
 * the background. This thread will also periodically (every 5 seconds) flush 
 * pending messages out to the server. 
 *
 * \param server_addr log server IP address
 * \param server_port log server port
 *
 * \return log client handle.  
 */
LIBCOM_API logClientId epicsStdCall logClientCreate (
    struct in_addr server_addr, unsigned short server_port);

/** \brief Log message
 *
 * Logs message to log server.  Messages are not immediately sent to the log 
 * server. Instead they are sent periodically (every 5 seconds), when the cache 
 * overflows, or when logClientFlush() is called. If messages can't sent, an error 
 * message will be printed to stderr
 *
 * \param id log client handle
 * \param message log message
 */
LIBCOM_API void epicsStdCall logClientSend (logClientId id, const char *message);

/** \brief Prints debug information about the log client state
 *
 * Print information about the log client's internal state
 * such as, connection status and cache state, to stdout
 *
 * \param id log client handle
 * \param level verbosity level. Level range is from 0 to 2
 */
LIBCOM_API void epicsStdCall logClientShow (logClientId id, unsigned level);

/** \brief Flushes all outstanding messages
 * 
 * Immediately sends all outstanding messages to the server
 *
 * \param id log client handle
 */
LIBCOM_API void epicsStdCall logClientFlush (logClientId id);

/** \brief Set prefix to be sent infront of every log message
 *
 * Sets a prefix to prepend every log message.  Can only be set
 * once.  If already set, this function call will be ignored
 *
 * \param prefix the prefix
 */
LIBCOM_API void epicsStdCall iocLogPrefix(const char* prefix);

/** \brief DEPRECATED
 * \deprecated deprecated interface; retained for backward compatibility 
 */
/* note: implementations are in iocLog.c, not logClient.c */
LIBCOM_API logClientId epicsStdCall logClientInit (void);

#ifdef __cplusplus
}
#endif

#endif /*INClogClienth*/
