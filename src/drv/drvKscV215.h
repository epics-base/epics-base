/* drvKscV215.c*/
/* base/src/drv $Id$ */
/*
 *      KscV215_driver.c
 *
 *      driver for KscV215 VXI module
 *
 *      Author:      Jeff Hill
 *      Date:        052192
 *
 *      Experimental Physics and Industrial Control System (EPICS)
 *
 *      Copyright 1991, the Regents of the University of California,
 *      and the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contracts:
 *      (W-7405-ENG-36) at the Los Alamos National Laboratory,
 *      and (W-31-109-ENG-38) at Argonne National Laboratory.
 *
 *      Initial development by:
 *              The Controls and Automation Group (AT-8)
 *              Ground Test Accelerator
 *              Accelerator Technology Division
 *              Los Alamos National Laboratory
 *
 *      Co-developed with
 *              The Controls and Computing Group
 *              Accelerator Systems Division
 *              Advanced Photon Source
 *              Argonne National Laboratory
 *
 *      Modification Log:
 *      -----------------
 *
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




