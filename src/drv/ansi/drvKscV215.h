/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* drvKscV215.c*/
/* base/src/drv $Id$ */
/*
 *      KscV215_driver.c
 *
 *      driver for KscV215 VXI module
 *
 *      Author:      Jeff Hill
 *      Date:        052192
 */

#define VXI_MODEL_KSCV215       (0x215)

typedef long 	kscV215Stat;

kscV215Stat     KscV215Init(void);

kscV215Stat KscV215_ai_driver(
unsigned        la,
unsigned        chan,
unsigned short  *prval
);

kscV215Stat KscV215_getioscanpvt(
unsigned        la,
IOSCANPVT       *scanpvt
);




