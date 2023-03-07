/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file epicsInterrupt.h
 * \brief Functions for interrupt handling.
 * \author likely Marty Kraimer
 *
 * Support for allocation of common device resources
 */
#ifndef epicsInterrupth
#define epicsInterrupth

#ifdef __cplusplus
extern "C" {
#endif

#include "libComAPI.h"

/** \brief This function disables interrupts and returns a key that can 
 * be used to restore the interrupt state later. */
LIBCOM_API int epicsInterruptLock(void);
/** \brief This function restores the interrupt state that was saved using
 *  epicsInterruptLock() */
LIBCOM_API void epicsInterruptUnlock(int key);
/** \brief This function returns a boolean value indicating whether the 
 * current context is an interrupt context. */
LIBCOM_API int epicsInterruptIsInterruptContext(void);
/** \brief This function logs a message indicating that the current context
 * is an interrupt context. */
LIBCOM_API void epicsInterruptContextMessage(const char *message);

#ifdef __cplusplus
}
#endif

#include "osdInterrupt.h"

#endif /* epicsInterrupth */
