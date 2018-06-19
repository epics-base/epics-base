/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#ifndef INC_osiNTPTime_H
#define INC_osiNTPTime_H

#ifdef __cplusplus
extern "C" {
#endif

void NTPTime_Init(int priority);
void NTPTime_Shutdown(void *dummy);
int  NTPTime_Report(int level);

#ifdef __cplusplus
}
#endif

#endif
