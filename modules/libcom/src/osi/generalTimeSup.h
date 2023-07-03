/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2008 Diamond Light Source Ltd
* Copyright (c) 2004 Oak Ridge National Laboratory
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/**
 * \file generalTimeSup.h
 * \brief The API provides functions and macros to register time providers and retrieve the current time or an event timestamp.
 * 
 */
#ifndef INC_generalTimeSup_H
#define INC_generalTimeSup_H

#include "epicsTime.h"
#include "epicsTimer.h"
#include "libComAPI.h"

/** \brief The priority value for a provider that should be used as a last resort when no other provider is available. */
#define LAST_RESORT_PRIORITY 999

#ifdef __cplusplus
extern "C" {
#endif

/** \brief A pointer to a function that takes a pointer to an epicsTimeStamp struct and returns an integer. */
typedef int (*TIMECURRENTFUN)(epicsTimeStamp *pDest);
/** \brief A pointer to a function that takes a pointer to an epicsTimeStamp struct and an integer, and returns an integer. */
typedef int (*TIMEEVENTFUN)(epicsTimeStamp *pDest, int event);

/** \brief Registers a function as a provider of the current time. */
LIBCOM_API int generalTimeRegisterCurrentProvider(const char *name,
    int priority, TIMECURRENTFUN getTime);
/** \brief Registers a function as a provider of an event timestamp. */
LIBCOM_API int generalTimeRegisterEventProvider(const char *name,
    int priority, TIMEEVENTFUN getEvent);

/** \brief Original names, for compatibility */
#define generalTimeCurrentTpRegister generalTimeRegisterCurrentProvider
/** \brief Original names, for compatibility */
#define generalTimeEventTpRegister generalTimeRegisterEventProvider

/** \brief Similar to generalTimeRegisterCurrentProvider, but adds the provider to the end of the list rather than sorting it by priority. */
LIBCOM_API int generalTimeAddIntCurrentProvider(const char *name,
    int priority, TIMECURRENTFUN getTime);
/** \brief Similar to generalTimeRegisterEventProvider, but adds the provider to the end of the list rather than sorting it by priority. */
LIBCOM_API int generalTimeAddIntEventProvider(const char *name,
    int priority, TIMEEVENTFUN getEvent);
/** \brief Retrieves the current time or an event timestamp from the provider with the highest priority lower than ignorePrio. */
LIBCOM_API int generalTimeGetExceptPriority(epicsTimeStamp *pDest,
    int *pPrio, int ignorePrio);

#ifdef __cplusplus
}
#endif

#endif /* INC_generalTimeSup_H */
