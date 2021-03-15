/*************************************************************************\
* Copyright (c) 2020 Michael Davidsaver
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef INC_dbLinkPvt_H
#define INC_dbLinkPvt_H

#include "dbLink.h"

#ifdef __cplusplus
extern "C" {
#endif

extern double dbLinkConnectionCheckDelay;

void dbLinkCheckStart(void);
void dbLinkConnCheck(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_dbLinkPvt_H */
