/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef INC_osiClockTime_H
#define INC_osiClockTime_H

#define CLOCKTIME_NOSYNC 0
#define CLOCKTIME_SYNC 1

#ifdef __cplusplus
extern "C" {
#endif

void ClockTime_Init(int synchronize);
void ClockTime_Shutdown(void *dummy);
int  ClockTime_Report(int level);

#ifdef __cplusplus
}
#endif

#endif
