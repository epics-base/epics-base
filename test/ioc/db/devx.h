/*************************************************************************\
* Copyright (c) 2014 Brookhaven Science Assoc. as Operator of Brookhaven
*     National Lab
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#ifndef DEVXSCANIO_H
#define DEVXSCANIO_H

#include <ellLib.h>
#include <dbScan.h>

#include <shareLib.h>

struct xRecord;
struct xpriv;

epicsShareExtern struct ELLLIST xdrivers;

typedef void (*xdrvcb)(struct xpriv *, void *);

typedef struct {
  ELLNODE drvnode;
  IOSCANPVT scan;
  xdrvcb cb;
  void *arg;
  ELLLIST privlist;
  int group;
} xdrv;

typedef struct xpriv {
  ELLNODE privnode;
  xdrv *drv;
  struct xRecord *prec;
  int member;
} xpriv;

epicsShareFunc xdrv *xdrv_add(int group, xdrvcb cb, void *arg);
epicsShareFunc xdrv *xdrv_get(int group);
epicsShareFunc void xdrv_reset();

typedef struct xdset {
    long      number;
    long (*report)(int);
    long (*init)(int);
    long (*init_record)(struct xRecord *);
    long (*get_ioint_info)(int, struct xRecord*, IOSCANPVT*);
    long (*process)(struct xRecord *);
} xdset;

#endif /* DEVXSCANIO_H */
