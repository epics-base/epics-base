/*************************************************************************\
* Copyright (c) 2012 ITER Organization
*
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Author:  Ralph Lange   Date:  26 Jun 2012 */

/* Null default thread hooks for all platforms that do not do anything special */

#include "epicsThread.h"

EPICS_THREAD_HOOK_ROUTINE epicsThreadHookDefault;
EPICS_THREAD_HOOK_ROUTINE epicsThreadHookMain;
