/***************************************************************************
 *   File:      generalTimeSup.h
 *   Author:        Peter Denison
 *   Institution:   Diamond Light Source
 *   Date:      04/2008
 *   Version:       1.1
 *
 *   support for general EPICS timestamp support
 * 
 *   This file should be included by Time Providers and contains the
 *   interface to the "lower" side of the generalTime framework
 *
 * Copyright (c) 2008 Diamond Light Source Ltd
 * EPICS BASE is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 *************************************************************************/
#ifndef generalTimeSuphInclude
#define generalTimeSuphInclude

#define LAST_RESORT_PRIORITY 999
#include <epicsTimer.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*pepicsTimeGetCurrent)(epicsTimeStamp *pDest);
typedef int (*pepicsTimeGetEvent)(epicsTimeStamp *pDest,int eventNumber);

int     generalTimeCurrentTpRegister(const char * desc, int priority, pepicsTimeGetCurrent getCurrent);
int     generalTimeEventTpRegister(const char * desc, int priority, pepicsTimeGetEvent getEvent);
int     generalTimeGetExceptPriority(epicsTimeStamp *pDest, int * pPriority, int except_tcp);
epicsTimerId    generalTimeCreateSyncTimer(epicsTimerCallback cb, void *param);

#ifdef __cplusplus
}
#endif
#endif /*generalTimeSuphInclude*/
