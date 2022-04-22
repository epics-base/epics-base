/*************************************************************************\
* Copyright (c) 2022 Michael Davidsaver
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <errno.h>

#include "epicsStdio.h"
#include "epicsMemFilePvt.h"

FILE* osdMemOpen(const epicsIMF* file, int binary)
{
    (void)file;
    (void)binary;
    errno = ENOTSUP;
    return NULL;
}
