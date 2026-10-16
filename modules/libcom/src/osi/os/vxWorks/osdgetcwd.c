/*************************************************************************\
* Copyright (c) 2025 Stanford University / SLAC National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
#include "osiFileName.h"

#include <stddef.h>
#include <ioLib.h>

char *epicsGetCwd(void)
{
    char *buf, *ret;
    buf = malloc(MAX_FILENAME_LENGTH+1);
    buf[MAX_FILENAME_LENGTH] = 0;
    if ((ret = getcwd(buf)))
        return ret;
    free(buf);
    return NULL;
}
