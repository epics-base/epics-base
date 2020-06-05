/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INC_osdThread_H
#define INC_osdThread_H

#ifdef __cplusplus
extern "C" {
#endif

int epicsThreadGetOssPriorityValue(unsigned int osiPriority);

#ifdef __cplusplus
}
#endif

#endif /* INC_osdThread_H */
