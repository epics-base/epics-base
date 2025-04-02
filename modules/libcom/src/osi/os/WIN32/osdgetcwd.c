/*************************************************************************\
* Copyright (c) 2025 Stanford University / SLAC National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
#include "osiFileName.h"

#include <direct.h>

char *epicsGetCwd(void)
{
    return _getcwd(NULL, 0);
}