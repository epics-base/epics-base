/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* Following needed for struct timeval */
#include <sys/times.h>

#ifndef osdTimeh
#define osdTimeh

#include <tickLib.h>
#include <sysLib.h>

#ifdef __cplusplus
extern "C" {
#endif

void osdNTPInit(void);
int  osdNTPGet(struct timespec *);
void osdNTPReport(void);
void osdClockReport(int level);

#define osdTickRateGet sysClkRateGet
#define osdTickGet tickGet
#define osdClockReport osdClockReport

#ifdef __cplusplus
}
#endif
#endif /* ifndef osdTimeh */
