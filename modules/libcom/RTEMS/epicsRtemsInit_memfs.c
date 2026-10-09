/*************************************************************************\
* Copyright (c) 2002 The University of Saskatchewan
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdio.h>

#include "epicsMemFs.h"

/* IOC override to load custom files */
const epicsMemFS* epicsRtemsFSImage __attribute__((weak)) = (void*) &epicsRtemsFSImage;

/* Needs to be the same in epicsRtemsInit.h */
int epicsRtemsMountLocalFilesystem(const char** argv) __attribute__((weak));
int epicsRtemsMountLocalFilesystem(const char** argv) {
    if (epicsRtemsFSImage == (void*) &epicsRtemsFSImage) {
        return -1; /* no FS image provided. */
    } else if (epicsRtemsFSImage == NULL) {
        return 0; /* no FS image provided, but none is needed. */
    } else {
        printf("***** Using compiled in file data *****\n");
        if (epicsMemFsLoad(epicsRtemsFSImage) != 0) {
            printf("error: can't unpack tar local filesystem\n");
            return -1;
        } else {
            printf("setting argv\n");
            argv[1] = "/";
            return 0;
        }
    }
}
