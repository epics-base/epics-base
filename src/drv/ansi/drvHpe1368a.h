/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* drvHpe1368a.h*/
/* base/src/drv $Id$ */

/*
 *      hpe1368a_driver.h
 *
 *      driver for hpe1368a and hpe1369a microwave switch VXI modules
 *
 *      Author:      Jeff Hill
 *      Date:        052192
 */

#define VXI_MODEL_HPE1368A      (0xf28)

typedef long    hpe1368aStat;

hpe1368aStat hpe1368a_init(void);

hpe1368aStat hpe1368a_getioscanpvt(
unsigned        la,
IOSCANPVT       *scanpvt
);

hpe1368aStat hpe1368a_bo_driver(
unsigned        la,
unsigned        val,
unsigned        mask
);

hpe1368aStat hpe1368a_bi_driver(
unsigned        la,
unsigned        mask,
unsigned        *pval
);

