/*************************************************************************\
* Copyright (c) 2021 Michael Davidsaver
* SPDX-License-Identifier: EPICS
* EPICS Base is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdarg.h>
#include <string.h>
#include <errno.h>

#include <epicsAssert.h>
#include <osiUnistd.h>
#include <epicsString.h>
#include "osiFileName.h"

#if defined(_WIN32) || defined(__CYGWIN__)
// cf. https://docs.microsoft.com/en-us/dotnet/standard/io/file-path-formats#canonicalize-separators
#  define SEP(C) (C=='/' || C=='\\')
#else
#  define SEP(C) (C=='/')
#endif

static const char magicCWD[] = "<current_directory>";
const char* const epicsPathJoinCurDir = magicCWD;

static
size_t epicsPathDF(char *path, size_t pathlen, int outfile)
{
    size_t pos;

    /* search forward to ignore unused buffer */
    pathlen = epicsStrnLen(path, pathlen);

    /* search backwards for last separator */
    pos = pathlen;
    for(; pos>0; pos--) {
        char c = path[pos-1u];
        if(SEP(c)) {
            /* found separator */
            if(outfile) {
                memcpy(path, &path[pos], pathlen-pos);
                pos = pathlen-pos;
            }
            goto done;
        }
    }
    /* no separator found */
    if(outfile) {
        pos = pathlen;
    }

done:
    if(pos < pathlen) {
        path[pos] = '\0';
    }

    return pos;
}

size_t epicsPathDir(char *path, size_t pathlen)
{
    return epicsPathDF(path, pathlen, 0);
}

size_t epicsPathFile(char *path, size_t pathlen)
{
    return epicsPathDF(path, pathlen, 1);
}

int epicsPathIsAbs(const char *path, size_t pathlen)
{
    /* for *NIX this is simple.  path[0]=='/' is absolute.  Anything else is relative
     *
     * for windows... not so much.
     * cf. https://docs.microsoft.com/en-us/dotnet/standard/io/file-path-formats#identify-the-path
     *
     * we treat as absolute:
     *  - device and UNC paths.
     *  - absolute on current drive.  eg. "\foo" -> "C:\foo" if %CD% starts with C:
     *  - absolute w/ drive.  eg. "C:\foo"
     *
     * we treat as relative
     *  - relative to some drive.  eg. "C:foo" -> "D:\bar\foo" if %CD% == "D:\bar"  (sneaky!)
     *  - anything else
     */
    if(pathlen>0 && SEP(path[0])) // *NIX: simple abs.  windows: device path, UNC path, or root of current drive
        return 1;
#if defined(_WIN32) || defined(__CYGWIN__)
    if(pathlen>=3 && ((path[0]>='a' && path[0]<='z') || (path[0]>='A' && path[0]<='Z')) && path[1]==':' && SEP(path[2])) {
        /* eg. C:FOO is relative
         *     C:/FOO is absolute
         */
        return 1;
    }
#endif
    return 0;
}

char* epicsPathJoin(const char **fragments, size_t nfragments)
{
    char* ret;
    char* cwd = NULL;
    size_t i;
    size_t firstfrag = 0;
    size_t nfrags = 0; /* nfragments excluding NULL and "" */
    size_t totallen = 0;

    STATIC_ASSERT(sizeof(OSI_PATH_SEPARATOR)==2);

    /* first pass to compute buffer size */
    for(i=0; i<nfragments; i++) {
        const char* frag = fragments[i];
        size_t flen;
        if(!frag || frag[0]=='\0')
            continue;

        if(frag==magicCWD) {
            if(!cwd)
                cwd = epicsPathAllocCWD();
            if(!cwd)
                return NULL;
            frag = cwd;
        }

        flen = strlen(frag);

        if(epicsPathIsAbs(frag, flen)) {
            /* reset */
            firstfrag = i;
            nfrags = 0;
            totallen = 0;
        }

        if(nfrags)
            totallen += 1; /* for separator */

        totallen += flen;
        nfrags++;
    }

    ret = malloc(totallen + 1);
    if(ret) {
        ret[0] = 0;

        for(i=firstfrag, nfrags=0; i<nfragments; i++) {
            const char* frag = fragments[i];

            if(!frag|| frag[0]=='\0')
                continue;

            if(frag==magicCWD) {
                assert(cwd);
                frag = cwd;
            }

            if(nfrags)
                strcat(ret, OSI_PATH_SEPARATOR);

            strcat(ret, frag);
            nfrags++;
        }
        assert(strlen(ret)==totallen);
    }

    free(cwd);
    return ret;
}

char* epicsPathAllocCWD(void)
{
    size_t bsize = 64u;
    char *buf = NULL;
    while(1) {
        char *temp = realloc(buf, bsize);
        if(!temp) {
            free(buf);
            buf = 0;
            break;
        }
        buf = temp;

        if(getcwd(buf, bsize)) {
            buf[bsize-1u] = '\0'; /* paranoia */
            break;

        } else if(errno==ERANGE && bsize<0x80000000) {
            bsize *= 2u;

        } else {
            free(buf);
            buf = 0;
            break;
        }
    }
    return buf;
}
