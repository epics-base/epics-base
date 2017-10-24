/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* devSup.h	Device Support		*/
/*
 *      Author:          Marty Kraimer
 *      Date:            6-1-90
 */

#ifndef INCdevSuph
#define INCdevSuph 1

#include "errMdef.h"
#include "shareLib.h"

/* structures defined elsewhere */
struct dbCommon;
struct devSup;
struct ioscan_head; /* aka IOSCANPVT */
struct link; /* aka DBLINK */

#ifdef __cplusplus
extern "C" {
    typedef long (*DEVSUPFUN)(void *);	/* ptr to device support function*/
#else
    typedef long (*DEVSUPFUN)();	/* ptr to device support function*/
#endif

typedef struct dset {   /* device support entry table */
    long	number;		/*number of support routines*/
    DEVSUPFUN	report;		/*print report*/
    DEVSUPFUN	init;		/*init support layer*/
    DEVSUPFUN	init_record;	/*init device for particular record*/
    DEVSUPFUN	get_ioint_info;	/* get io interrupt information*/
    /*other functions are record dependent*/
} dset;

/** Type safe alternative to 'struct dset'
 *
 * Recommended usage
 @code
 long my_drv_init_record(dbCommon *prec);
 long my_drv_get_iointr_info(int deattach, dbCommon *prec, IOCSCANPVT* pscan);
 long my_longin_read(longinRecord *prec);
 typedef struct {
    typed_dset common;
    long (*read)(longinRecord *prec);
 } my_longin_dset;
 static const my_longin_dset devLiMyDrvName = {{
      5, // 4 from typed_dset + 1 more
      NULL,
      NULL,
      &my_drv_init_record,
      &my_drv_get_iointr_info
    },
    &my_longin_read
 };
 epicsExportAddress(dset,  devLiMyDrvName);
 @endcode
 */
typedef struct typed_dset {
    /** Number of function pointers which follow.  Must be >=4 */
    long number;
    /** Called from dbior() */
    long (*report)(int lvl);
    /** Called twice during iocInit().
     * First with phase=0 early, before init_record() and array field alloc.
     * Again with phase=1 after init_record()
     */
    long (*init)(int phase);
    /** Called once per record instance */
    long (*init_record)(struct dbCommon *prec);
    /** Called when SCAN="I/O Intr" on startup, or after SCAN is changed.
     *
     * Caller must assign third arguement (IOCSCANPVT*).  eg.
     @code
     struct mpvt {
        IOSCANPVT drvlist;
     };
     ...
        // init code calls
        scanIoInit(&pvt->drvlist);
     ...
     long my_get_ioint_info(int deattach, struct dbCommon *prec, IOCSCANPVT* pscan) {
        if(prec->dpvt)
            *pscan = &((mypvt*)prec->dpvt)->drvlist;
     @endcode
     *
     * When a particular record instance can/will only used a single scan list, then
     * the 'detach' argument should be ignored.
     *
     * If this is not the case, then the following should be noted.
     * get_ioint_info() called with deattach=0 to fetch the scan list to which this record will be added.
     * Again with detach=1 to fetch the scan list from which this record will be removed.
     * Calls will be balanced.
     * A call with detach=0 will be matched by a call with detach=1.
     *
     * @note get_ioint_info() will be called during IOC shutdown if the del_record()
     *       extended callback is provided.  (from 3.15.0.1)
     */
    long (*get_ioint_info)(int deattach, struct dbCommon *prec, struct ioscan_head** pscan);
    /*other functions are record dependent*/
} typed_dset;

typedef struct dsxt {   /* device support extension table */
    long (*add_record)(struct dbCommon *precord);
    long (*del_record)(struct dbCommon *precord);
    /* Recordtypes are *not* allowed to extend this table */
} dsxt;

/** Fetch INP or OUT link (or NULL if record type has neither).
 *
 * Recommended for use in device support init_record()
 */
epicsShareFunc struct link* dbGetDevLink(struct dbCommon* prec);

epicsShareExtern dsxt devSoft_DSXT;  /* Allow anything table */

epicsShareFunc void devExtend(dsxt *pdsxt);
epicsShareFunc void dbInitDevSup(struct devSup *pdevSup, dset *pdset);


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

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
