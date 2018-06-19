/*************************************************************************\
* Copyright (c) 2016 Michael Davidsaver
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
 \*************************************************************************/

#include <stdlib.h>
#include <string.h>

#include <dbAccess.h>
#include <link.h>
#include <alarm.h>
#include <dbJLink.h>
#include <dbDefs.h>
#include <dbConvertFast.h>
#include <epicsMutex.h>
#include <epicsAtomic.h>
#include <epicsUnitTest.h>

#define epicsExportSharedSymbols

#include "jlinkz.h"

#include <epicsExport.h>

int numzalloc;


static
void z_open(struct link *plink)
{
    zpriv *priv = CONTAINER(plink->value.json.jlink, zpriv, base);

    if(priv->isopen)
        testDiag("lsetZ re-open");
    priv->isopen = 1;
    testDiag("Open jlinkz %p", priv);
}

static
void z_remove(struct dbLocker *locker, struct link *plink)
{
    zpriv *priv = CONTAINER(plink->value.json.jlink, zpriv, base);

    epicsMutexLock(priv->lock);

    if(!priv->isopen)
        testDiag("lsetZ remove without open");

    epicsMutexUnlock(priv->lock);

    testDiag("Remove/free jlinkz %p", priv);

    epicsAtomicDecrIntT(&numzalloc);

    epicsMutexDestroy(priv->lock);
    free(priv);
    plink->value.json.jlink = NULL; /* paranoia */
}

static
int z_connected(const struct link *plink)
{
    return 1; /* TODO: not provided should be connected */
}

static
int z_dbftype(const struct link *plink)
{
    return DBF_LONG;
}

static
long z_elements(const struct link *plink, long *nelements)
{
    *nelements = 1;
    return 0;
}

static
long z_getval(struct link *plink, short dbrType, void *pbuffer,
        long *pnRequest)
{
    long ret;
    long (*pconv)(const epicsInt32 *, void *, const dbAddr *) = dbFastGetConvertRoutine[DBF_LONG][dbrType];
    zpriv *priv = CONTAINER(plink->value.json.jlink, zpriv, base);

    if(pnRequest && *pnRequest==0) return 0;

    epicsMutexLock(priv->lock);
    ret = (*pconv)(&priv->value, pbuffer, NULL);
    epicsMutexUnlock(priv->lock);
    if(ret==0 && pnRequest) *pnRequest = 1;
    return ret;
}

/* TODO: atomicly get value and alarm */
static
long z_getalarm(const struct link *plink, epicsEnum16 *status,
        epicsEnum16 *severity)
{
    zpriv *priv = CONTAINER(plink->value.json.jlink, zpriv, base);
    epicsEnum16 sevr, stat;

    epicsMutexLock(priv->lock);
    sevr = priv->isset ? 0 : INVALID_ALARM;
    stat = priv->isset ? 0 : LINK_ALARM;
    epicsMutexUnlock(priv->lock);

    if(status)   *status = stat;
    if(severity) *severity = sevr;
    return 0;
}

static
long z_putval(struct link *plink, short dbrType,
        const void *pbuffer, long nRequest)
{
    long ret;
    long (*pconv)(epicsInt32 *, const void *, const dbAddr *) = dbFastPutConvertRoutine[DBF_LONG][dbrType];
    zpriv *priv = CONTAINER(plink->value.json.jlink, zpriv, base);

    if(nRequest==0) return 0;

    epicsMutexLock(priv->lock);
    ret = (*pconv)(&priv->value, pbuffer, NULL);
    epicsMutexUnlock(priv->lock);
    return ret;
}

static lset lsetZ = {
    0, 0, /* non-const, non-volatile */
    &z_open,
    &z_remove,
    NULL, NULL, NULL, /* load */
    &z_connected,
    &z_dbftype,
    &z_elements,
    &z_getval,
    NULL, /* control limits */
    NULL, /* display limits */
    NULL, /* alarm limits */
    NULL, /* prec */
    NULL, /* units */
    &z_getalarm,
    NULL, /* time */
    &z_putval,
    NULL, /* putasync */
    NULL, /* forward */
    NULL, /* doLocked */
};

static
jlink* z_alloc(short dbfType)
{
    zpriv *priv;
    priv = calloc(1, sizeof(*priv));
    if(!priv) goto fail;

    priv->lock = epicsMutexCreate();
    if(!priv->lock) goto fail;

    epicsAtomicIncrIntT(&numzalloc);

    testDiag("Alloc jlinkz %p", priv);

    return &priv->base;
fail:
    if(priv && priv->lock) epicsMutexDestroy(priv->lock);
    free(priv);
    return NULL;
}

static
void z_free(jlink *pj)
{
    zpriv *priv = CONTAINER(pj, zpriv, base);

    if(priv->isopen)
        testDiag("lsetZ jlink free after open()");

    testDiag("Free jlinkz %p", priv);

    epicsAtomicDecrIntT(&numzalloc);

    epicsMutexDestroy(priv->lock);
    free(priv);
}

static
jlif_result z_int(jlink *pj, long long num)
{
    zpriv *priv = CONTAINER(pj, zpriv, base);

    priv->value = num;
    priv->isset = 1;

    return jlif_continue;
}

static
jlif_key_result z_start(jlink *pj)
{
    return jlif_key_continue;
}

static
jlif_result z_key(jlink *pj, const char *key, size_t len)
{
    zpriv *priv = CONTAINER(pj, zpriv, base);

    if(len==4 && strncmp(key,"fail", len)==0) {
        testDiag("Found fail key jlinkz %p", priv);
        return jlif_stop;
    } else {
        return jlif_continue;
    }
}

static
jlif_result z_end(jlink *pj)
{
    return jlif_continue;
}

static
struct lset* z_lset(const jlink *pj)
{
    return &lsetZ;
}

static jlif jlifZ = {
    "z",
    &z_alloc,
    &z_free,
    NULL, /* null */
    NULL, /* bool */
    &z_int,
    NULL, /* double */
    NULL, /* string */
    &z_start,
    &z_key,
    &z_end,
    NULL, /* start array */
    NULL, /* end array */
    NULL, /* end child */
    &z_lset,
    NULL, /* report */
    NULL  /* map child */
};

epicsExportAddress(jlif, jlifZ);
