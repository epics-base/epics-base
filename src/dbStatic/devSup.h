/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devSup.h	Device Support		*/
/* $Id$ */
/*
 *      Author:          Marty Kraimer
 *      Date:            6-1-90
 */

#ifndef INCdevSuph
#define INCdevSuph 1

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
typedef long (*DEVSUPFUN)(void*);	/* ptr to device support function*/
#else
typedef long (*DEVSUPFUN)();	/* ptr to device support function*/
#endif

typedef struct dset {	/* device support entry table */
    long	number;		/*number of support routines*/
    DEVSUPFUN	report;		/*print report*/
    DEVSUPFUN	init;		/*init support layer*/
    DEVSUPFUN	init_record;	/*init device for particular record*/
    DEVSUPFUN	get_ioint_info;	/* get io interrupt information*/
    /*other functions are record dependent*/
}dset;

struct dbCommon;
typedef struct dsxt {   /* device support extension table */
    long (*add_record)(struct dbCommon *precord);
    long (*del_record)(struct dbCommon *precord);
    /* Recordtypes are *not* allowed to extend this table */
} dsxt;

#define S_dev_noDevSup      (M_devSup| 1) /*SDR_DEVSUP: Device support missing*/
#define S_dev_noDSET        (M_devSup| 3) /*Missing device support entry table*/
#define S_dev_missingSup    (M_devSup| 5) /*Missing device support routine*/
#define S_dev_badInpType    (M_devSup| 7) /*Bad INP link type*/
#define S_dev_badOutType    (M_devSup| 9) /*Bad OUT link type*/
#define S_dev_badInitRet    (M_devSup|11) /*Bad init_rec return value */
#define S_dev_badBus        (M_devSup|13) /*Illegal bus type*/
#define S_dev_badCard       (M_devSup|15) /*Illegal or nonexistant module*/
#define S_dev_badSignal     (M_devSup|17) /*Illegal signal*/
#define S_dev_NoInit        (M_devSup|19) /*No init*/
#define S_dev_Conflict      (M_devSup|21) /*Multiple records accessing same signal*/
#define S_dev_noDeviceFound (M_devSup|23) /*No device found at specified address*/


/* These are defined in src/misc/iocInit.c */

epicsShareDef struct dsxt devSoft_DSXT;
epicsShareFunc void devExtend(dsxt *pdsxt);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
