/*************************************************************************\
* Copyright (c) 2016 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* dbDbLink.h
 *
 *  Created on: April 3rd, 2016
 *      Author: Andrew Johnson
 */

#ifndef INC_dbDbLink_H
#define INC_dbDbLink_H

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

struct link;
struct dbLocker;

epicsShareFunc long dbDbInitLink(struct link *plink, short dbfType);
epicsShareFunc void dbDbAddLink(struct dbLocker *locker, struct link *plink,
    short dbfType, DBADDR *ptarget);

#ifdef __cplusplus
}
#endif

#endif /* INC_dbDbLink_H */
