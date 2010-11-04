
/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Revision-Id$ */
/*
 *      Author:		Jeff Hill 
 */

#include <cygwin/version.h>

#define epicsExportSharedSymbols
#include "osiSock.h"

enum epicsSocketSystemCallInterruptMechanismQueryInfo 
        epicsSocketSystemCallInterruptMechanismQuery ()
{
#if (CYGWIN_VERSION_DLL_MAJOR >= 1007)
    // Behaviour changed in Cygwin 1.7 release.
    return esscimqi_socketCloseRequired;
#else
    return esscimqi_socketBothShutdownRequired;
#endif
}
