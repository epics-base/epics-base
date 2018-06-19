/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

/*
 * Author: Eric Norum
 */

#ifndef osdTimeh
#define osdTimeh

/*
 * Linux needs this include file since the POSIX version
 * causes `struct timespec' to be defined in more than one place.
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

epicsShareFunc void epicsShareAPI
    convertDoubleToWakeTime(double timeout,struct timespec *wakeTime);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ifndef osdTimeh */

