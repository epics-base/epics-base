/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 *      Author:     Jeff Hill
 */

#include <cygwin/version.h>

#include "osiSock.h"

enum epicsSocketSystemCallInterruptMechanismQueryInfo
        epicsSocketSystemCallInterruptMechanismQuery ()
{
#if (CYGWIN_VERSION_DLL_MAJOR == 1007) && (CYGWIN_VERSION_DLL_MINOR < 15)
    // Behavior changed in early Cygwin 1.7 releases, reverted later.
    return esscimqi_socketCloseRequired;
#else
    return esscimqi_socketBothShutdownRequired;
#endif
}
