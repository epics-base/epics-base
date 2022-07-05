/*************************************************************************\
* Copyright (c) 2012 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH.
* Copyright (c) 2012 ITER Organization.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file epicsSpin.h
 *
 * \brief OS independent interface for creating spin locks
 *
 * OS independent interface for creating spin locks. Implemented using the 
 * OS-specific spinlock interface if available. Otherwise uses epicsMutexLock.
 */
#ifndef epicsSpinh
#define epicsSpinh

#include "libComAPI.h"

#ifdef __cplusplus
extern "C" {
#endif


/** Handle to spin lock*/
typedef struct epicsSpin *epicsSpinId;

/** \brief Creates a spin lock
 *
 * Creates a spin lock
 *
 * \return Handle to spinlock or NULL if failed 
 *    to create the lock
 */
LIBCOM_API epicsSpinId epicsSpinCreate(void);

/** \brief Creates a spin lock
 *
 * Creates a spin lock. Calls cantProceed() if unable 
 * to create lock
 */
LIBCOM_API epicsSpinId epicsSpinMustCreate(void);

/** \brief Destroys spin lock
 *  
 *  Destroys the spin lock
 *
 * \param lockId identifies the spinlock
 */
LIBCOM_API void epicsSpinDestroy(epicsSpinId lockId);

/** \brief Acquires the spin lock
 * 
 * Acquires the lock.  Blocks if lock is unavailable 
 *
 * \param lockId  identifies the spinlock
*/
LIBCOM_API void epicsSpinLock(epicsSpinId lockId);

/** \brief Tries to acquire the spin lock    
 *
 * Tries to acquire the lock. If failed, return immediately
 * with non-zero error code.
 *
 * \param lockId identifies the spinlock
 * 
 * \return 0 if lock was acquired.  1 if failed because acquired by another thread.
 * Otherwise returns non-zero OS specific return code if failed for any other reason
 */
LIBCOM_API int epicsSpinTryLock(epicsSpinId lockId);

/** \brief Releases spin lock
 *
 * Releases spin lock
 * 
 * \param lockId identifies the spinlock
 */
LIBCOM_API void epicsSpinUnlock(epicsSpinId lockId);

#ifdef __cplusplus
}
#endif

#endif /* epicsSpinh */
