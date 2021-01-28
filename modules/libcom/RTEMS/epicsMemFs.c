/*************************************************************************\
* Copyright (c) 2014 Brookhaven National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include <osiFileName.h>
#include "epicsMemFs.h"

#ifndef PATH_MAX
#  define PATH_MAX 100
#endif

int epicsMemFsLoad(const epicsMemFS *fs)
{
    char* initdir;
    const epicsMemFile * const *fileptr = fs->files;

    initdir = epicsPathAllocCWD();

    for(;*fileptr; fileptr++) {
        const epicsMemFile *curfile = *fileptr;
        int fd;
        ssize_t ret;
        size_t sofar;
        const char * const *dir = curfile->directory;
        /* jump back to the root each time,
         * slow but simple.
         */
        if(chdir(initdir)) {
            perror("chdir");
            goto onerrno;
        }

        printf("-> /");

        /* traverse directory tree, creating as necessary */
        for(;*dir; dir++) {
            int ret;
            if(**dir=='.') continue; /* ignore '.' and '..' */
            printf("%s/", *dir);
            ret = chdir(*dir);
            if(ret==-1 && errno==ENOENT) {
                /* this directory doesn't exist */
                if(mkdir(*dir,0744)==-1) {
                    printf("\n");
                    perror("mkdir");
                    goto onerrno;
                }
                if(chdir(*dir)==-1) {
                    printf("\n");
                    perror("chdir2");
                    goto onerrno;
                }
            } else if(ret==-1) {
                printf("\n");
                perror("chdir1");
                goto onerrno;
            }
        }

        /* no file name creates an empty directory */
        if(!curfile->name) {
            printf("\n");
            continue;
        }
        printf("%s", curfile->name);

        /* create or overwrite */
        fd = open(curfile->name, O_WRONLY|O_CREAT|O_TRUNC, 0644);

        if(fd==-1) {
            printf("\n");
            perror("open");
            goto onerrno;
        }

        sofar = 0;

        while(sofar<curfile->size) {
            ret = write(fd, curfile->data+sofar, curfile->size-sofar);
            if(ret<=0) {
                printf("\n");
                perror("write");
                goto onerrno;
            }
            sofar += ret;
        }

        close(fd);
        printf(" - ok\n");
    }

    if(chdir(initdir))
        perror("chdir");

    free(initdir);
    return 0;
onerrno:
    free(initdir);
    return errno;
}
