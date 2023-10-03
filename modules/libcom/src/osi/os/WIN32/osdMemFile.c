/*************************************************************************\
* Copyright (c) 2022 Michael Davidsaver
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#define VC_EXTRALEAN
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <io.h>

#include "epicsStdio.h"
#include "epicsString.h"
#include "epicsMemFilePvt.h"

#ifndef _WIN32
// help qtcreator
typedef unsigned long DWORD;
typedef unsigned UINT;
#define MAX_PATH (128u)
#define O_TEMPORARY (0u)

DWORD GetLastError(void);
DWORD GetTempPath(DWORD, char*);
UINT GetTempFileName(const char*, const char*, UINT, char*);
#endif

FILE* osdMemOpen(const epicsIMF* file, int binary)
{
    FILE* ret = NULL;
    DWORD tmpdirlen = GetTempPath(0, NULL);
    size_t tmpalloc;
    char* tmp = NULL;
    char* actualalloc = NULL;

    if(!tmpdirlen) {
        errno = EINVAL;
        goto done;
    }

    /* eg. "C:\Temp\" "emf" + "XXXXXXXX" + ".tmp" + '\0'
     */
    tmpalloc = tmpdirlen + 3 + 8 + 4 + 1;
    tmp = malloc(tmpalloc);
    if(!tmp) {
        errno = ENOMEM;
        goto done;
    }

    tmpdirlen = GetTempPath(tmpdirlen, tmp);
    if(!tmpdirlen) {
        errno = GetLastError();
        goto done;
    }

    while(1) {
        int r = rand();
        int fd;
        int n = -1;
        const char* actual = NULL;

        epicsSnprintf(tmp+tmpdirlen,
                      tmpalloc-tmpdirlen-1u,
                      "emf%08x.tmp",
                      r);

        fd = open(tmp,
                  O_CREAT | O_EXCL | O_TEMPORARY | O_RDWR | (binary ? O_BINARY : O_TEXT),
                  _S_IREAD | _S_IWRITE);
        if(fd==-1) {
            if(errno==EEXIST)
                continue; /* re-try with a different name */
            else
                break;
        }

        switch(file->encoding) {
        case epicsIMFRaw:
            actual = file->content;
            break;
        case epicsIMFZ:
            actual = actualalloc = malloc(file->actualLen);
            if(actualalloc) {
                if(!!(errno = osiUncompressZ(file->content,
                                             file->contentLen,
                                             actualalloc,
                                             file->actualLen))) {
                    free(actualalloc);
                    actual = actualalloc = NULL;
                }
            }

        default:
            errno = EIO;
        }

        if(actual) {
            n = write(fd, actual, file->actualLen);
        }

        if(n!=file->actualLen
           || lseek(fd, 0, SEEK_SET)
           || !(ret = fdopen(fd, binary ? "rb" : "r"))) {
            close(fd);
        }
        break;
    }

done:
    free(tmp);
    free((char*)actualalloc);
    return ret;
}
