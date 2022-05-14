/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/*
 * Author: Jeff Hill
 */

#ifndef osdTimeh
#define osdTimeh

#include <unistd.h>

#if !defined(_POSIX_TIMERS) || _POSIX_TIMERS < 0
    struct timespec {
        time_t tv_sec; /* seconds since some epoch */
        long tv_nsec; /* nanoseconds within the second */
    };
#endif /* !_POSIX_TIMERS */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

LIBCOM_API void epicsStdCall
    convertDoubleToWakeTime(double timeout,struct timespec *wakeTime);

#ifdef __rtems__
void osdNTPInit(void);
int  osdNTPGet(struct timespec *now);
int osdTickGet(void);
int  osdTickRateGet(void);
void osdNTPReport(void);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ifndef osdTimeh */

