/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2008 Diamond Light Source Ltd
* Copyright (c) 2004 Oak Ridge National Laboratory
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INC_epicsGeneralTime_H
#define INC_epicsGeneralTime_H

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NUM_TIME_EVENTS 256
/* Time Events numbered 0 through (NUM_TIME_EVENTS-1) are validated by */
/* code in epicsGeneralTime.c that tests for advancing timestamps and */
/* enforces that restriction.  Event numbers greater than or equal to */
/* NUM_TIME_EVENTS are now allowed if supported by a custom time provider */
/* which should provide its own advancing timestamp validation. */

epicsShareFunc void generalTime_Init(void);

epicsShareFunc int  installLastResortEventProvider(void);

epicsShareFunc void generalTimeResetErrorCounts(void);
epicsShareFunc int  generalTimeGetErrorCounts(void);

epicsShareFunc const char * generalTimeCurrentProviderName(void);
epicsShareFunc const char * generalTimeEventProviderName(void);
epicsShareFunc const char * generalTimeHighestCurrentName(void);

/* Original names, for compatibility */
#define generalTimeCurrentTpName generalTimeCurrentProviderName
#define generalTimeEventTpName generalTimeEventProviderName

epicsShareFunc long generalTimeReport(int interest);

#ifdef __cplusplus
}
#endif

#endif /* INC_epicsGeneralTime_H */
