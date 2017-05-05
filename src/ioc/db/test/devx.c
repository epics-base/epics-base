/*************************************************************************\
* Copyright (c) 2014 Brookhaven Science Assoc. as Operator of Brookhaven
*     National Lab
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <string.h>
#include <stdio.h>

#include <epicsAssert.h>
#include <cantProceed.h>
#include <ellLib.h>
#include <dbDefs.h>
#include <dbAccess.h>
#include <dbScan.h>
#include <devSup.h>
#include <recGbl.h>
#include <link.h>
#include <dbLink.h>
#include <xRecord.h>

#include <epicsExport.h>

#include "devx.h"

/* xRecord DTYP="Scan I/O"
 *
 * dset to test I/O Intr scanning.
 * INP="@drvname" names a "driver" which
 * provides a IOSCANPVT.
 * The driver also defines a callback function which
 * is invoked when the record is processed.
 */

struct ELLLIST xdrivers;

/* Add a new "driver" with the given group id
 * and processing callback
 */
xdrv* xdrv_add(int group, xdrvcb cb, void *arg)
{
    xdrv *drv=callocMustSucceed(1, sizeof(*drv), "xdrv_add");
    drv->cb = cb;
    drv->arg = arg;
    drv->group = group;
    scanIoInit(&drv->scan);
    ellAdd(&xdrivers, &drv->drvnode);
    return drv;
}

/* Trigger the named "driver" group to scan */
xdrv *xdrv_get(int group)
{
    ELLNODE *cur;
    for(cur=ellFirst(&xdrivers); cur; cur=ellNext(cur)) {
        xdrv *curd = CONTAINER(cur, xdrv, drvnode);
        if(curd->group==group) {
            return curd;
        }
    }
    cantProceed("xdrv_get() for non-existant group");
    return NULL;
}

/* Free all "driver" groups and record private structures.
 * Call after testdbCleanup()
 */
void xdrv_reset()
{
    ELLNODE *cur;
    while((cur=ellGet(&xdrivers))!=NULL) {
        ELLNODE *cur2;
        xdrv *curd = CONTAINER(cur, xdrv, drvnode);
        while((cur2=ellGet(&curd->privlist))!=NULL) {
            xpriv *priv = CONTAINER(cur2, xpriv, privnode);
            free(priv);
        }
        free(curd);
    }
}

static long xscanio_init_record(xRecord *prec)
{
    ELLNODE *cur;
    xpriv *priv;
    xdrv *drv = NULL;
    int group, member;
    assert(prec->inp.type==INST_IO);

    if(sscanf(prec->inp.value.instio.string, "%d %d",
              &group, &member)!=2)
        cantProceed("xscanio_init_record invalid INP string");

    for(cur=ellFirst(&xdrivers); cur; cur=ellNext(cur)) {
        xdrv *curd = CONTAINER(cur, xdrv, drvnode);
        if(curd->group==group) {
            drv = curd;
            break;
        }
    }

    assert(drv!=NULL);
    priv = mallocMustSucceed(sizeof(*priv), "xscanio_init_record");
    priv->prec = prec;
    priv->drv = drv;
    priv->member = member;
    ellAdd(&drv->privlist, &priv->privnode);
    prec->dpvt = priv;

    return 0;
}

static long xscanio_get_ioint_info(int cmd, xRecord *prec, IOSCANPVT *ppvt)
{
    xpriv *priv = prec->dpvt;
    if(!priv || !priv->drv)
        return 0;
    *ppvt = priv->drv->scan;
    return 0;
}

static long xscanio_read(xRecord *prec)
{
    xpriv *priv = prec->dpvt;
    if(!priv || !priv->drv)
        return 0;
    if(priv->drv->cb)
        (*priv->drv->cb)(priv, priv->drv->arg);
    return 0;
}

static xdset devxScanIO = {
    5, NULL, NULL,
    &xscanio_init_record,
    &xscanio_get_ioint_info,
    &xscanio_read
};
epicsExportAddress(dset, devxScanIO);

/* basic DTYP="Soft Channel" */
static long xsoft_init_record(xRecord *prec)
{
    recGblInitConstantLink(&prec->inp, DBF_LONG, &prec->val);
    return 0;
}

static long xsoft_read(xRecord *prec)
{
    return dbGetLink(&prec->inp, DBR_LONG, &prec->val, NULL, NULL);
}

static struct xdset devxSoft = {
    5, NULL, NULL,
    &xsoft_init_record,
    NULL,
    &xsoft_read
};
epicsExportAddress(dset, devxSoft);
