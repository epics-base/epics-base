/*************************************************************************\
* Copyright (c) 2014 Brookhaven National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
#ifndef EPICSMEMFS_H
#define EPICSMEMFS_H

#include <stdlib.h>

typedef struct {
    const char * const *directory; /* NULL terminated list of directories */
    const char *name; /* file name */
    const char *data; /* file contents */
    size_t size; /* size of file contents in bytes */
} epicsMemFile;

typedef struct {
    const epicsMemFile * const *files;
} epicsMemFS;

#ifdef __cplusplus
extern "C" {
#endif

int epicsMemFsLoad(const epicsMemFS *fs);

#ifdef __cplusplus
}
#endif

#endif // EPICSMEMFS_H
