/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file asTrapWrite.h
 * \brief API for monitoring external put operations to an IOC.
 * \author  Marty Kraimer
 *
 * The access security subsystem provides an API asTrapWrite that makes
 * put/write requests visible to any facility that registers a listener.
 */

#ifndef INCasTrapWriteh
#define INCasTrapWriteh

#include "dbCoreAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dbChannel;

/**
 * \brief The message passed to registered listeners.
 */
typedef struct asTrapWriteMessage {
    const char *userid; /**< \brief Userid of whoever originated the request. */
    const char *hostid; /**< \brief Hostid of whoever originated the request. */
    /** \brief A field for use by the server.
     *
     * Any listener that uses this field must know what type of
     * server is forwarding the put requests. This pointer holds
     * the value the server provides to asTrapWriteWithData(), which
     * for RSRV is the dbChannel pointer for the target field.
     */
    struct dbChannel *serverSpecific;
    /** \brief A field for use by the \ref asTrapWriteListener.
     *
     * When the listener is called before the write, this has the
     * value 0. The listener can give it any value it desires
     * and it will have the same value when the listener gets
     * called again after the write.
     */
    void *userPvt;
    int dbrType; /**< \brief Data type from ca/db_access.h, NOT dbFldTypes.h */
    int no_elements; /**< \brief Array length  */
    void *data;     /**< \brief Might be NULL if no data is available */
} asTrapWriteMessage;

/**
 * \brief An identifier needed to unregister an listener.
 */
typedef void *asTrapWriteId;

/**
 * \brief Pointer to a listener function.
 *
 * Each registered listener function is called twice for every put; once
 * before and once after the write is performed.
 * The listener may set \c userPvt in the first call and retrieve it in the
 * second call.
 *
 * Each asTrapWriteMessage can change or may be deleted after the user's
 * asTrapWriteListener returns
 *
 * The listener function is called by a server thread so it must not block
 * or do anything that causes a delay.
*/
typedef void(*asTrapWriteListener)(asTrapWriteMessage *pmessage,int after);

/**
 * \brief Register function to be called on asTrapWriteListener.
 * \param func The listener function to be called.
 * \return A listener identifier for unregistering this listener.
 */
DBCORE_API
asTrapWriteId epicsStdCall asTrapWriteRegisterListener(
    asTrapWriteListener func);
/**
 * \brief Unregister asTrapWriteListener.
 * \param id Listener identifier from asTrapWriteRegisterListener().
 */
DBCORE_API
void epicsStdCall asTrapWriteUnregisterListener(
    asTrapWriteId id);

DBCORE_API
void * epicsStdCall asTrapWriteBeforeWithData(
    const char *userid, const char *hostid, struct dbChannel *addr,
    int dbrType, int no_elements, void *data);

DBCORE_API
void epicsStdCall asTrapWriteAfterWrite(void *pvt);

/* More convenience macros
void *asTrapWriteWithData(ASCLIENTPVT asClientPvt,
     const char *userid, const char *hostid, void *addr,
     int dbrType, int no_elements, void *data);
void asTrapWriteAfter(ASCLIENTPVT asClientPvt);
*/
#define asTrapWriteWithData(asClientPvt, user, host, addr, type, count, data) \
    ((asActive && (asClientPvt)->trapMask) \
    ? asTrapWriteBeforeWithData((user), (host), (addr), (type), (count), (data)) \
    : 0)
#define asTrapWriteAfter(pvt) \
    if (pvt) asTrapWriteAfterWrite(pvt)

/* This macro is for backwards compatibility, upgrade any code
   calling it to use asTrapWriteWithData() instead ASAP:
void *asTrapWriteBefore(ASCLIENTPVT asClientPvt,
     const char *userid, const char *hostid, void *addr);
*/
#define asTrapWriteBefore(asClientPvt, user, host, addr) \
    asTrapWriteWithData(asClientPvt, user, host, addr, 0, 0, NULL)

#ifdef __cplusplus
}
#endif

#endif /*INCasTrapWriteh*/
