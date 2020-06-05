/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* Copyright (c) 2012 ITER Organization.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/**
 * \file epicsRingBytes.h
 * \author Marty Kraimer, Eric Norum, Ralph Lange
 * \brief A circular buffer to store bytes
 *
 * \details
 * EpicsRingBytes provides a C API for creating and using ring buffers
 * (first in first out circular buffers) that store bytes. The unlocked
 * variant is designed so that one writer thread and one reader thread
 * can access the ring simultaneously without requiring mutual exclusion.
 * The locked variant uses an epicsSpinLock, and works with any numbers of
 * writer and reader threads.
 * \note If there is only one writer it is not necessary to lock for puts.
 * If there is a single reader it is not necessary to lock for gets.
 * epicsRingBytesLocked uses a spinlock.
 */

#ifndef INCepicsRingBytesh
#define INCepicsRingBytesh

#ifdef __cplusplus
extern "C" {
#endif

#include "libComAPI.h"

/** \brief An identifier for a ring buffer */
typedef void *epicsRingBytesId;
typedef void const *epicsRingBytesIdConst;

/**
 * \brief Create a new ring buffer
 * \param nbytes Size of ring buffer to create
 * \return Ring buffer Id or NULL on failure
 */
LIBCOM_API epicsRingBytesId  epicsStdCall epicsRingBytesCreate(int nbytes);
/**
 * \brief Create a new ring buffer, secured by a spinlock
 * \param nbytes Size of ring buffer to create
 * \return Ring buffer Id or NULL on failure
 */
LIBCOM_API epicsRingBytesId  epicsStdCall epicsRingBytesLockedCreate(int nbytes);
/**
 * \brief Delete the ring buffer and free any associated memory
 * \param id RingbufferID returned by epicsRingBytesCreate()
 */
LIBCOM_API void epicsStdCall epicsRingBytesDelete(epicsRingBytesId id);
/**
 * \brief Read data out of the ring buffer
 * \param id RingbufferID returned by epicsRingBytesCreate()
 * \param value Where to put the data fetched from the buffer
 * \param nbytes Maximum number of bytes to get
 * \return The number of bytes actually fetched
 */
LIBCOM_API int  epicsStdCall epicsRingBytesGet(
    epicsRingBytesId id, char *value,int nbytes);
/**
 * \brief Write data into the ring buffer
 * \param id RingbufferID returned by epicsRingBytesCreate()
 * \param value Source of the data to be put into the buffer
 * \param nbytes How many bytes to put
 * \return The number of bytes actually stored, zero if not enough space
 */
LIBCOM_API int  epicsStdCall epicsRingBytesPut(
    epicsRingBytesId id, char *value,int nbytes);
/**
 * \brief Make the ring buffer empty
 * \param id RingbufferID returned by epicsRingBytesCreate()
 * \note Should only be used when both gets and puts are locked out.
 */
LIBCOM_API void epicsStdCall epicsRingBytesFlush(epicsRingBytesId id);
/**
 * \brief Return the number of free bytes in the ring buffer
 * \param id RingbufferID returned by epicsRingBytesCreate()
 * \return The number of free bytes in the ring buffer
 */
LIBCOM_API int  epicsStdCall epicsRingBytesFreeBytes(epicsRingBytesId id);
/**
 * \brief Return the number of bytes currently stored in the ring buffer
 * \param id RingbufferID returned by epicsRingBytesCreate()
 * \return The number of bytes currently stored in the ring buffer
 */
LIBCOM_API int  epicsStdCall epicsRingBytesUsedBytes(epicsRingBytesId id);
/**
 * \brief Return the size of the ring buffer
 * \param id RingbufferID returned by epicsRingBytesCreate()
 * \return Return the size of the ring buffer, i.e., nbytes specified in
 * the call to epicsRingBytesCreate().
 */
LIBCOM_API int  epicsStdCall epicsRingBytesSize(epicsRingBytesId id);
/**
 * \brief Test if the ring buffer is currently empty.
 * \param id RingbufferID returned by epicsRingBytesCreate()
 * \return 1 if the buffer is empty, otherwise 0
 */
LIBCOM_API int  epicsStdCall epicsRingBytesIsEmpty(epicsRingBytesId id);
/**
 * \brief Test if the ring buffer is currently full.
 * \param id RingbufferID returned by epicsRingBytesCreate()
 * \return 1 if the buffer is full, otherwise 0
 */
LIBCOM_API int  epicsStdCall epicsRingBytesIsFull(epicsRingBytesId id);
/**
 * \brief See how full a ring buffer has been since it was last checked.
 *
 * Returns the maximum amount of data the ring buffer has held in bytes
 * since the water mark was last reset. A new ring buffer starts with a
 * water mark of 0.
 * \param id RingbufferID returned by epicsRingBytesCreate()
 * \return Actual Highwater mark
 */
LIBCOM_API int  epicsStdCall epicsRingBytesHighWaterMark(epicsRingBytesIdConst id);
/**
 * \brief Reset the Highwater mark of the ring buffer.
 *
 * The Highwater mark will be set to the current usage
 * \param id RingbufferID returned by epicsRingBytesCreate()
 */
LIBCOM_API void epicsStdCall epicsRingBytesResetHighWaterMark(epicsRingBytesId id);

#ifdef __cplusplus
}
#endif


#endif /* INCepicsRingBytesh */
