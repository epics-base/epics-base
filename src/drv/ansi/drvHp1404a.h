/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* base/src/drv $Id$ */
/*
 * drvHp1404a.h 
 *
 *      HP E1404A VXI bus slot zero translator
 *      device dependent routines header file
 *
 *      share/src/drv/@(#)drvHp1404a.h	1.1 8/27/93
 *
 *      Author Jeffrey O. Hill
 *      Date            030692
 */

typedef long    hpE1404Stat;

hpE1404Stat hpE1404Init(void);

hpE1404Stat hpE1404SignalConnect(
        unsigned        la,
        void            (*pSignalCallback)(int16_t signal)
);

hpE1404Stat hpE1404RouteTriggerECL(
unsigned        la,             /* slot zero device logical address     */
unsigned        enable_map,     /* bits 0-5  correspond to trig 0-5     */
                                /* a 1 enables a trigger                */
                                /* a 0 disables a trigger               */
unsigned        io_map         /* bits 0-5  correspond to trig 0-5     */
                                /* a 1 sources the front panel          */
                                /* a 0 sources the back plane           */
);

hpE1404Stat hpE1404RouteTriggerTTL(
unsigned        la,             /* slot zero device logical address     */
unsigned        enable_map,     /* bits 0-5  correspond to trig 0-5     */
                                /* a 1 enables a trigger                */
                                /* a 0 disables a trigger               */
unsigned        io_map         /* bits 0-5  correspond to trig 0-5     */
                                /* a 1 sources the front panel          */
                                /* a 0 sources the back plane           */
);

#define VXI_HP_MODEL_E1404_REG_SLOT0    0x10
#define VXI_HP_MODEL_E1404_REG          0x110
#define VXI_HP_MODEL_E1404_MSG          0x111


