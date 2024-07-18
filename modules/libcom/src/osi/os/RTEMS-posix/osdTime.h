/*************************************************************************\
* Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INC_osdTime_H
#define INC_osdTime_H

#ifdef __cplusplus
extern "C" {
#endif

void osdTimeRegister(void);

void osdNTPInit(void);
int  osdNTPGet(struct timespec *);
void osdNTPReport(void);

int  osdTickRateGet(void);
int  osdTickGet(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_osdTime_H */
