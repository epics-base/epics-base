/*************************************************************************\
* Copyright (c) 2022 Michael Davidsaver
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Link support for File System access
 *
 * {fs:"/path/to/some.file"}
 * {fs:{path:"/path/to/some.file"}}
 *
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include <dbDefs.h>
#include <errlog.h>
#include <epicsStdio.h>
#include <epicsString.h>
#include <dbLink.h>
#include <dbJLink.h>
#include <dbAccess.h>
#include <recGbl.h>
#include <alarm.h>
#include <epicsExport.h>

typedef enum {
    ps_unused,
    ps_path,
    ps_ignore = 0xff,
} fs_parse_state;

/* parser state stack.
 * max 4 frames.  f[0] is inner-most.
 *
 * Unused frames are ps_unused (aka. zero)
 */
typedef union {
    epicsUInt32 all;
    epicsUInt8 f[4];
} fs_parse_stack;

typedef struct {
    jlink link;

    const char *path;

    dbCommon *prec;

    FILE *fp;

    epicsUInt8 *buf;
    size_t bufpos;
    size_t buflen;

    fs_parse_stack pstack;

    unsigned create:1;
    unsigned writable:1;
    unsigned binary:1;
    unsigned append:1;
    //unsigned lines:1;
} fs_link;

#define JL2FS(LINK) CONTAINER(LINK, fs_link, link)
#define LK2FS(LINK) CONTAINER(plink->value.json.jlink, fs_link, link)

static
long fs_errno(const struct link *plink)
{
    long ret;
    const char *msg = strerror(ret = errno);

    recGblSetSevrMsg(plink->precord, LINK_ALARM, INVALID_ALARM,
                     "%s:%d:%s",
                     dbLinkFieldName(plink), (int)ret, msg);
    return ret;
}

static
long fs_ensure_open(const struct link *plink)
{
    fs_link *fs = LK2FS(plink);

    if(!fs->fp) {
        const char *mode;
        if(!fs->writable) {
            mode = fs->binary ? "rb" : "r";
        } else {
            if(fs->append) {
                mode = "a";
            } else {
                mode = fs->create ? "w" : "r+";
            }
        }
        FILE *fp = fopen(fs->path, mode);
        if(!fp)
            return fs_errno(plink);

        fs->fp = fp;
    }
    return 0;
}

static
void fs_open(struct link *plink) {
    (void)plink;
}

static
void fs_remove(struct dbLocker *locker, struct link *plink) {
    fs_link *fs = LK2FS(plink);
    (void)locker;

    if(fs->fp) {
        (void)fclose(fs->fp);
    }
    free((char*)fs->path);
    free(fs);
}

static
int fs_isConnected(const struct link *plink)
{
    fs_link *fs = LK2FS(plink);

    (void)fs_ensure_open(plink);

    return !!fs->fp;
}

static
int fs_getDBFtype(const struct link *plink)
{
    (void)plink;
    return DBF_CHAR;
}

static
long fs_getElements(const struct link *plink, long *pnElements)
{
    (void)plink;
    // TODO: how can we even estimate this...
    *pnElements = MAX_STRING_SIZE;
    return 0;
}

static
long fs_getValue(struct link *plink, short dbrType, void *pbuffer,
                 long *pnRequest)
{
    fs_link *fs = LK2FS(plink);
    long ret = fs_ensure_open(plink);

    if(!ret) {
        size_t nReq = pnRequest ? (size_t)*pnRequest : 1u;

        int iodirect = (dbrType==DBR_STRING && nReq==1)
                  || ( (dbrType==DBR_UCHAR || dbrType==DBR_CHAR) && nReq>1u );

        if(iodirect) { /* read directly into caller buffer */
            char *pbuf = pbuffer;
            size_t blen = dbrType==DBR_STRING ? MAX_STRING_SIZE : nReq;

            assert(blen>1); /* should only be reachable if DBR_STRING or nReq>1 */
            blen--; /* we will always terminate w/ nil */

            epicsInt64 nread = fread(pbuf, 1, blen, fs->fp);
            if( nread < 0 ) {
                ret = fs_errno(plink);

            } else if(iodirect) {
                pbuf[nread] = '\0';
                if(dbrType==DBR_STRING)
                    nread = 1;
                if(pnRequest)
                    *pnRequest = nread;
            }

        } else {
            char *pbuf = pbuffer;
            size_t n;
            size_t esize = dbValueSize(dbrType);

            for(n=0; !ret && n<nReq; n++, pbuf+=esize) {
                long err;

                switch(dbrType) {
                case DBR_CHAR:
                case DBR_SHORT:
                case DBR_LONG:
                case DBR_INT64: {
                    long long t;
                    err = fscanf(fs->fp, "%lld", &t);
                    if(err==1) {
                        switch(dbrType) {
                        case DBR_CHAR:  *(epicsInt8 *)pbuf = t; break;
                        case DBR_SHORT: *(epicsInt16*)pbuf = t; break;
                        case DBR_LONG:  *(epicsInt32*)pbuf = t; break;
                        case DBR_INT64: *(epicsInt64*)pbuf = t; break;
                        }
                    } else {
                        ret = fs_errno(plink);
                    }
                }
                    break;
                case DBR_UCHAR:
                case DBR_USHORT:
                case DBR_ULONG:
                case DBR_UINT64: {
                    unsigned long long t;
                    err = fscanf(fs->fp, "%llu", &t);
                    if(err==1) {
                        switch(dbrType) {
                        case DBR_UCHAR:  *(epicsUInt8 *)pbuf = t; break;
                        case DBR_USHORT: *(epicsUInt16*)pbuf = t; break;
                        case DBR_ULONG:  *(epicsUInt32*)pbuf = t; break;
                        case DBR_UINT64: *(epicsUInt64*)pbuf = t; break;
                        }
                    } else {
                        ret = fs_errno(plink);
                    }
                }
                    break;
                case DBR_FLOAT:
                case DBR_DOUBLE: {
                    double t;
                    err = fscanf(fs->fp, "%lg", &t);
                    if(err==1) {
                        switch(dbrType) {
                        case DBR_FLOAT:  *(float *)pbuf = t; break;
                        case DBR_DOUBLE: *(double*)pbuf = t; break;
                        }
                    } else {
                        ret = fs_errno(plink);
                    }
                }
                    break;
                case DBR_STRING:
                    ret = fscanf(fs->fp, "%39c", pbuf);
                    if(ret==1) {
                        pbuf[MAX_STRING_SIZE-1] = '\0';
                    } else {
                        ret = fs_errno(plink);
                    }
                default:
                    ret = S_db_badDbrtype;
                }
            }

            if(!ret && pnRequest)
                *pnRequest = n;
        }
    }

    if(fs->fp) {
        (void)fclose(fs->fp);
        fs->fp = NULL;
    }

    return ret;
}

static
long fs_loadScalar(struct link *plink, short dbrType, void *pbuffer)
{
    return fs_getValue(plink, dbrType, pbuffer, NULL);
}

static
long fs_loadLS(struct link *plink, char *pbuffer, epicsUInt32 size,
               epicsUInt32 *plen)
{
    long cap = size;
    long ret = fs_getValue(plink, DBR_CHAR, pbuffer, &cap);
    if(!ret)
        *plen = cap;
    return ret;
}

static
long fs_putValue(struct link *plink, short dbrType,
                 const void *pbuffer, long nRequest)
{
    fs_link *fs = LK2FS(plink);
    long ret = fs_ensure_open(plink);

    if(!ret) {
        int iodirect = (dbrType==DBR_STRING && nRequest==1)
                  || ( (dbrType==DBR_UCHAR || dbrType==DBR_CHAR) && nRequest!=1u );

        if(iodirect) {
            if(dbrType==DBR_STRING)
                nRequest = epicsStrnLen(pbuffer, MAX_STRING_SIZE);

            epicsInt64 n = fwrite(pbuffer, 1, nRequest, fs->fp);
            if( n < 0) {
                ret = fs_errno(plink);
            }
        } else {
            const char *pbuf = pbuffer;
            long n;
            size_t esize = dbValueSize(dbrType);

            for(n=0; !ret && n<nRequest; n++, pbuf+=esize) {
                long err;

                if(n!=0) {
                    err = fputc(' ', fs->fp);
                    if(err) {
                        ret = fs_errno(plink);
                        break;
                    }
                }

                switch(dbrType) {
                case DBR_CHAR:
                case DBR_SHORT:
                case DBR_LONG:
                case DBR_INT64: {
                    long long t;
                    switch(dbrType) {
                    case DBR_CHAR:  t = *(epicsInt8 *)pbuf; break;
                    case DBR_SHORT: t = *(epicsInt16*)pbuf; break;
                    case DBR_LONG:  t = *(epicsInt32*)pbuf; break;
                    case DBR_INT64: t = *(epicsInt64*)pbuf; break;
                    }
                    err = fprintf(fs->fp, "%lld", t);
                    if(err<0)
                        ret = fs_errno(plink);
                }
                    break;
                case DBR_UCHAR:
                case DBR_USHORT:
                case DBR_ULONG:
                case DBR_UINT64: {
                    unsigned long long t;
                    switch(dbrType) {
                    case DBR_UCHAR:  t = *(epicsUInt8 *)pbuf; break;
                    case DBR_USHORT: t = *(epicsUInt16*)pbuf; break;
                    case DBR_ULONG:  t = *(epicsUInt32*)pbuf; break;
                    case DBR_UINT64: t = *(epicsUInt64*)pbuf; break;
                    }
                    err = fprintf(fs->fp, "%llu", t);
                    if(err<0)
                        ret = fs_errno(plink);
                }
                    break;
                case DBR_FLOAT:
                case DBR_DOUBLE: {
                    double t;
                    switch(dbrType) {
                    case DBR_FLOAT:  t = *(float *)pbuf; break;
                    case DBR_DOUBLE: t = *(double*)pbuf; break;
                    }
                    err = fprintf(fs->fp, "%gd", t);
                    if(err<0)
                        ret = fs_errno(plink);
                }
                    break;
                case DBR_STRING:
                    err = fprintf(fs->fp, "%40s", pbuf);
                    if(err<0)
                        ret = fs_errno(plink);
                    break;
                default:
                    ret = S_db_badDbrtype;
                }
            }
        }
    }

    if(fs->fp) {
        (void)fclose(fs->fp);
        fs->fp = NULL;
    }

    return ret;
}

static lset lnkFS_lset = {
    0,
    1,
    fs_open,
    fs_remove,
    fs_loadScalar,
    fs_loadLS,
    fs_getValue,
    fs_isConnected,
    fs_getDBFtype,
    fs_getElements,
    fs_getValue,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    fs_putValue,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

static
jlink* fs_parse_alloc(short dbfType)
{
    fs_link *fs;

    if (dbfType == DBF_FWDLINK) {
        errlogPrintf("lnkFS: No support for forward links\n");
        return NULL;
    }

    if(!(fs = calloc(1, sizeof(*fs)))) {
        return NULL;
    }

    if(dbfType==DBF_OUTLINK)
        fs->writable = 1;

    fs->create = 1;
    fs->append = 1;

    return &fs->link;
}

static
void fs_parse_free(jlink *link)
{
    fs_link *fs = JL2FS(link);

    assert(!fs->fp); /* never set during parsing */
    free((char*)fs->path);
    free(fs);
}

static
jlif_result fs_parse_string(jlink *link, const char *val, size_t len)
{
    fs_link *fs = JL2FS(link);

    if(!fs->pstack.all || fs->pstack.f[0]==ps_path) {
        char *path = malloc(len+1);
        if(!path)
            return 0;
        memcpy(path, val, len);
        path[len] = '\0';

        fs->path = path;
        return 1;

    } else if(fs->pstack.f[0]==ps_ignore) {
        return 1;
    } else {
        return 0; /* wrong type for known key */
    }
}

static
jlif_key_result fs_part_start_map(jlink *link)
{
    fs_link *fs = JL2FS(link);

    if(fs->pstack.f[3]!=ps_unused)
        return 0; /* too deep */

    fs->pstack.all <<= 8u;
    fs->pstack.f[0] = ps_ignore;
    return 1;
}

static
jlif_result fs_part_map_key(jlink *link, const char *key, size_t len)
{
    fs_link *fs = JL2FS(link);

    if(!(fs->pstack.all&0xffffff00)) { /* depth 1 */
        if(len==4 && memcmp(key, "path", 4)==0) {
            fs->pstack.f[0] = ps_path;

        } else {
            fs->pstack.f[0] = ps_ignore;
        }
    }
    return 1;
}

static
jlif_result fs_part_end_map(jlink *link)
{
    fs_link *fs = JL2FS(link);

    fs->pstack.all >>= 8u;

    if(!fs->pstack.all) {
        if(!fs->path)
            return 0; /* missing required */
    }
    return 1;
}

static
lset* fs_parse_lset(const jlink *link)
{
    (void)link;
    return &lnkFS_lset;
}

static
void fs_parse_report(const jlink *link, int level, int indent)
{
    fs_link *fs = JL2FS(link);
    (void)level;

    printf("%*s'fs': path = \"%s\"\n", indent, "", fs->path);
}

static jlif lnkFSIf = {
    "fs",
    fs_parse_alloc, fs_parse_free,
    NULL,
    NULL,
    NULL,
    NULL,
    fs_parse_string,
    fs_part_start_map,
    fs_part_map_key,
    fs_part_end_map,
    NULL,
    NULL,
    NULL,
    fs_parse_lset,
    fs_parse_report,
    NULL,
    NULL,
};
epicsExportAddress(jlif, lnkFSIf);
