/* drvHpe1368a.h*/
/* base/src/drv $Id$ */

/*
 *      hpe1368a_driver.h
 *
 *      driver for hpe1368a and hpe1369a microwave switch VXI modules
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

