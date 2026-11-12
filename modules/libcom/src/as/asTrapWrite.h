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

#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

struct dbChannel;

/**
 * \brief The message passed to registered listeners.
 */
typedef struct asTrapWriteMessage {
    const char *userid; /**< \brief Userid of whoever originated the request. */
    const char *method; /**< \brief How the account was authenticated, e.g.
                         * "anonymous", "ca", or "x509". May be NULL or empty if the
                         * forwarding server does not supply one; listeners must
                         * tolerate both. */
    const char *authority; /**< \brief Who vouches for the account. For the "x509"
                         * method this is the common name of the root certificate
                         * authority; for methods without a certificate (e.g.
                         * "anonymous", "ca") it is NULL or empty. Listeners must
                         * tolerate both. */
    const char *hostid; /**< \brief Hostid of whoever originated the request. */
    epicsInt8 protocol; /**< \brief Transport security of the connection, as an
                         * \c AsProtocol value (see asLib.h): \c AS_PROTOCOL_TCP (0)
                         * for plain TCP or \c AS_PROTOCOL_TLS (1) for TLS. Servers
                         * that predate transport security report \c AS_PROTOCOL_TCP. */
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
LIBCOM_API asTrapWriteId epicsStdCall asTrapWriteRegisterListener(
    asTrapWriteListener func);
/**
 * \brief Unregister asTrapWriteListener.
 * \param id Listener identifier from asTrapWriteRegisterListener().
 */
LIBCOM_API void epicsStdCall asTrapWriteUnregisterListener(
    asTrapWriteId id);

#ifdef __cplusplus
}
#endif

#endif /*INCasTrapWriteh*/
