/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
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

LIBCOM_API void epicsStdCall
    convertDoubleToWakeTime(double timeout,struct timespec *wakeTime);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ifndef osdTimeh */

