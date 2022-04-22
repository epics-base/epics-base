/*************************************************************************\
* Copyright (c) 2022 Michael Davidsaver
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <stdio.h>
#include <errno.h>
#include <string.h>

#define EPICS_PRIVATE_API

#include <dbDefs.h>
#include <epicsString.h>
#include <epicsMutex.h>
#include <epicsThread.h>
#include <cantProceed.h>
#include <errlog.h>
#include <gpHash.h>
#include <iocsh.h>

#include "epicsStdio.h"
#include "epicsMemFilePvt.h"

#ifdef IMF_HAS_ZLIB
#  include <zlib.h>
#endif

#define IMFPREFIX "app://"

typedef struct epicsIMFPvt {
    ELLNODE node;
    size_t nref;
    epicsIMF imf;
} epicsIMFPvt;

typedef struct {
    struct gphPvt* table;

    epicsMutexId lock;
    ELLLIST list;
    unsigned iterate:1;
} eimfs_t;

static eimfs_t *eimfs;

static epicsThreadOnceId eimfsOnce = EPICS_THREAD_ONCE_INIT;

static
void epicsMemInit(void* unused);

void epicsIMFInc(const epicsIMF *mf)
{
    epicsIMFPvt *pvt = CONTAINER((epicsIMF*)mf, epicsIMFPvt, imf);

    epicsMutexMustLock(eimfs->lock);

    assert(pvt->nref>0u);
    pvt->nref++;

    epicsMutexUnlock(eimfs->lock);
}

void epicsIMFDec(const epicsIMF *mf)
{
    if(mf) {
        size_t nref;
        epicsIMFPvt *pvt = CONTAINER((epicsIMF*)mf, epicsIMFPvt, imf);

        epicsMutexMustLock(eimfs->lock);

        assert(pvt->nref > 0u);
        nref = --pvt->nref;

        epicsMutexUnlock(eimfs->lock);

        if(!nref) {
            free(pvt);
        }
    }
}

FILE* epicsMemOpen(const char *pathname, const char *mode)
{
    FILE *ret = NULL;
    GPHENTRY *ent;
    char c;
    int binary = 0;

    for(c=*mode++; c; c=*mode++) {
        switch(c) {
        case ' ':
        case 'r':
            break;
        case 'b':
            binary = 1;
            break;
        default:
            errno = EINVAL;
            return NULL;
        }
    }

    epicsThreadOnce(&eimfsOnce, &epicsMemInit, NULL);

    ent = gphFind(eimfs->table, pathname, eimfs);
    if(ent) {
        epicsIMFPvt *pvt = (epicsIMFPvt*)ent->userPvt;
        ret = osdMemOpen(&pvt->imf, binary);

    } else {
        errno = ENOENT;
    }
    return ret;
}

/* eimfs->lock is held */
static
int mountFile(const epicsIMF *f, unsigned verb)
{
    int ret;
    const size_t pathlen = strlen(f->path);
    epicsIMFPvt *pvt = NULL;
    size_t nalloc = sizeof(*pvt) + pathlen + 1u;
    char *pbuf;
    GPHENTRY *ent;

    /* validation */

    if(f->path[0]!='/' || f->path[pathlen-1u]=='/') {
        if(verb)
            fprintf(stderr, ERL_WARNING " path must begin, but not end, with '/' \"%s\"\n",
                    f->path);
        ret = EINVAL;
        goto done;
    }

    switch(f->encoding) {
    case epicsIMFRaw:
    case epicsIMFZ:
#ifndef IMF_HAS_ZLIB
        if(verb)
            fprintf(stderr, ERL_WARNING " zlib not available.  unusable file : \"%s\"\n",
                    f->path);
#endif
        break;
    default:
        if(verb)
            fprintf(stderr, ERL_WARNING " EMF unknown compression %u for \"%s\"\n",
                    f->encoding, f->path);

        /* continue, file will be unreadable */
    }

    /* allocate and populate epicsIMFPvt */

    if(!f->nocopy)
        nalloc += f->contentLen;

    pvt = malloc(nalloc);
    if(!pvt) {
        fprintf(stderr, "Unable to allocate space for '%s'\n", f->path);
        ret = ENOMEM;
        goto done;
    }
    pvt->nref = 1u;

    memcpy(&pvt->imf, f, sizeof(pvt->imf));

    pbuf = (char*)pvt;
    pbuf += sizeof(*pvt);
    pvt->imf.path = pbuf;
    memcpy(pbuf, f->path, pathlen+1u);

    if(!f->nocopy) {
        pbuf += pathlen+1u;
        memcpy(pbuf, f->content, f->contentLen);
        pvt->imf.content = pbuf;
    }

    /* silently replace existing */
    (void)epicsMemUnmount(pvt->imf.path, 0);

    if(!!(ent = gphAdd(eimfs->table, pvt->imf.path, eimfs))) {

        ent->userPvt = pvt;
        ellAdd(&eimfs->list, &pvt->node);
        pvt->nref++;

        ret = 0;

    } else {
        if(verb)
            fprintf(stderr, ERL_WARNING " EMF not enough memory for \"%s\"\n",
                    pvt->imf.path);

        ret = ENOMEM;
    }

done:
    epicsIMFDec(&pvt->imf);
    return ret;
}

int epicsMemMount(const epicsIMF* files,
                  epicsUInt64 flags)
{
    int ret = 0;
    const unsigned verb = !!(flags&EPICS_MEM_MOUNT_VERBOSE);
    size_t n;

    if(flags&0xfe00)
        return ENOTSUP;

    epicsThreadOnce(&eimfsOnce, &epicsMemInit, NULL);

    epicsMutexMustLock(eimfs->lock);

    if(eimfs->iterate) {
        ret = EPERM;

    } else {
        for(n=0u; files[n].path && !ret; n++) {
            ret = mountFile(&files[n], verb);
        }
    }

    epicsMutexUnlock(eimfs->lock);

    return ret;
}

int epicsMemUnmount(const char *path, epicsUInt64 flags)
{
    int ret;
    GPHENTRY *ent = NULL;

    if(flags&0xffff)
        return ENOTSUP;

    epicsThreadOnce(&eimfsOnce, &epicsMemInit, NULL);

    epicsMutexMustLock(eimfs->lock);

    if(eimfs->iterate) {
        ret = EPERM;

    } else {
        ent = gphFind(eimfs->table, path, eimfs);
    }

    if(ent) {
        epicsIMFPvt *prev = ent->userPvt;
        ent->userPvt = NULL;

        gphDelete(eimfs->table, prev->imf.path, eimfs);
        ellDelete(&eimfs->list, &prev->node);
        epicsIMFDec(&prev->imf);
        ret = 0;

    } else {
        ret = ENOENT;
    }

    epicsMutexUnlock(eimfs->lock);

    return ret;
}


void epicsMemFileForEach(void (*func)(void *arg, const epicsIMF* mf),
                         void *arg)
{
    ELLNODE *cur;
    epicsThreadOnce(&eimfsOnce, &epicsMemInit, NULL);

    epicsMutexMustLock(eimfs->lock);

    eimfs->iterate = 1;

    for(cur = ellFirst(&eimfs->list); cur; cur = ellNext(cur)) {
        epicsIMFPvt* pvt = CONTAINER(cur, epicsIMFPvt, node);

        (*func)(arg, &pvt->imf);
    }

    eimfs->iterate = 0;

    epicsMutexUnlock(eimfs->lock);
}

FILE* epicsFOpen(const char *pathname, const char *mode)
{
    if(strncmp(IMFPREFIX, pathname, sizeof(IMFPREFIX)-1)==0) {
        pathname += sizeof(IMFPREFIX)-1u;
        return epicsMemOpen(pathname, mode);

    } else {
        return fopen(pathname, mode);
    }
}

/* cf. https://www.zlib.net/manual.html
 */
int osiUncompressZ(const char* inbuf, size_t inlen, char *outbuf, size_t outlen)
{
#ifdef IMF_HAS_ZLIB
    uLongf dst = outlen;
    int err = uncompress((Bytef*)outbuf, &dst, (const Bytef*)inbuf, inlen);
    if(err!=Z_OK) {
        switch(err) {
        case Z_MEM_ERROR: err = ENOMEM; break; /* alloc error */
        case Z_BUF_ERROR: err = ENOMEM; break; /* outlen was wrong */
        case Z_DATA_ERROR: err = EIO; break; /* corrupt */
        case Z_ERRNO: break;
        default: err = EIO;
        }
        free(outbuf);
        outbuf = NULL;
    }

    return err;
#else
    return EIO;
#endif /* IMF_HAS_ZLIB */
}

static
void showMemFile(void *unused, const epicsIMF* mf)
{
    const epicsIMFPvt *pvt = CONTAINER(mf, epicsIMFPvt, imf);
    char mode[4];

    mode[0] = 'R';
    mode[3] = '\0';

    switch(mf->encoding) {
    case epicsIMFRaw: mode[1] = 'R'; break;
#ifdef IMF_HAS_ZLIB
    case epicsIMFZ: mode[1] = 'Z'; break;
#else
    case epicsIMFZ: mode[1] = 'z'; break;
#endif
    default: mode[1] = '?';
    }

    mode[2] = mf->nocopy ? ' ' : 'A';

    epicsStdoutPrintf("%s %u %lu " IMFPREFIX "%s\n", mode,
                      (unsigned)pvt->nref, (unsigned long)mf->actualLen,
                      mf->path);
}

void epicsMemFileShow(void)
{
#ifndef IMF_HAS_ZLIB
    printf("# Compression not supported\n");
#endif
    epicsMemFileForEach(&showMemFile, NULL);
}

static const iocshFuncDef epicsMemFileShowFuncDef = {
    "epicsMemFileShow", 0, 0,
    "List currently mounted in-memory files\n"
};

static void epicsMemFileShowCallFunc (const iocshArgBuf *args)
{
    epicsMemFileShow();
}

static
void epicsMemInit(void* unused)
{
    (void)unused;
    eimfs = callocMustSucceed(1, sizeof(*eimfs), "eimfs");
    eimfs->lock = epicsMutexMustCreate();
    gphInitPvt(&eimfs->table, 32u);
    ellInit(&eimfs->list);
    iocshRegister(&epicsMemFileShowFuncDef, &epicsMemFileShowCallFunc);
}
