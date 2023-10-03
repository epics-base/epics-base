/*************************************************************************\
* Copyright (c) 2022 Michael Davidsaver
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#if defined(__linux__)
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif
#  define USE_FOPENCOOKIE

#else
/* freebsd, OSX, RTEMS */
#  define USE_FUNOPEN
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "epicsStdio.h"
#include "epicsMemFilePvt.h"

#if defined(USE_FOPENCOOKIE)
typedef size_t loc_t;
typedef ssize_t sloc_t;
typedef cookie_io_functions_t io_functions_t;

#define xfopen(COOKIE, FUNCS) fopencookie(COOKIE, "r", FUNCS)

#elif defined(USE_FUNOPEN)
typedef fpos_t seek_off_t;
typedef int loc_t;
typedef int sloc_t;

typedef struct {
    int (*close)(void *raw);
    int (*read)(void *raw, char* buf, loc_t cnt);
} io_functions_t;

#define xfopen(COOKIE, FUNCS) funopen(COOKIE, (FUNCS).read, NULL, NULL, (FUNCS).close)
#endif

typedef struct {
    const epicsIMF* mf;
    size_t pos;
    const char* actual;
} mfCookie;

static
int mfClose(void *raw)
{
    mfCookie *c = (mfCookie*)raw;

    epicsIMFDec(c->mf);
    free(c);

    return 0;
}

static
sloc_t mfRead(void *raw, char* buf, loc_t cnt)
{
    mfCookie *c = (mfCookie*)raw;
    size_t remaining = c->mf->actualLen - c->pos;

    if(cnt > remaining)
        cnt = remaining;

    memcpy(buf, c->actual + c->pos, cnt);
    c->pos += cnt;

    return cnt;
}

static
const io_functions_t mf = {
    .close = mfClose,
    .read = mfRead,
};

FILE* osdMemOpen(const epicsIMF* file, int binary)
{
    FILE* ret = NULL;
    mfCookie *c;
    size_t asize = sizeof(*c);

    (void)binary; /* no relevant on *NIX */

    if(file->encoding == epicsIMFZ)
        asize += file->actualLen;

    c = calloc(1, asize);

    if(!c) {
        errno = ENOMEM;

    } else {
        const io_functions_t *pio = NULL;
        char *buf;

        c->mf = file;

        switch (file->encoding) {
        case epicsIMFRaw:
            pio = &mf;
            c->actual = c->mf->content;
            break;
        case epicsIMFZ:
            buf = (char*)&c[1];
            if(!osiUncompressZ(c->mf->content,
                              c->mf->contentLen,
                              buf,
                              c->mf->actualLen))
            {
                c->actual = buf;
                pio = &mf;

            } else {
                errno = EIO;
            }
            break;
        default:
            errno = EIO;
        }

        if(pio)
            ret = xfopen(c, *pio);
    }

    if(ret) {
        epicsIMFInc(file);

    } else {
        free(c);
    }
    return ret;
}
