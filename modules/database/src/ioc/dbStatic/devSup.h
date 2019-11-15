/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/** @file devSup.h
 *
 * @brief Device support routines
 */

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
typedef struct ioscan_head *IOSCANPVT;
struct link; /* aka DBLINK */

/** Type safe version of 'struct dset'
 *
 * Recommended usage:
 *
 * In Makefile:
 @code
 USR_CFLAGS += -DUSE_TYPED_RSET -DUSE_TYPED_DSET
 @endcode
 *
 * In C source file:
 @code
 #include <devSup.h>
 #include <dbScan.h>      // For IOCSCANPVT
 ...
 #include <epicsExport.h> // defines epicsExportSharedSymbols
 ...
 static long init_record(dbCommon *prec);
 static long get_iointr_info(int detach, dbCommon *prec, IOCSCANPVT* pscan);
 static long longin_read(longinRecord *prec);

 const struct {
     dset common;
     long (*read)(longinRecord *prec);
 } devLiDevName = {
     {
      5, // 4 from dset + 1 from longinRecord
         NULL,
         NULL,
         &init_record,
         &get_iointr_info
     },
     &longin_read
 };
 epicsExportAddress(dset, devLiDevName);
 @endcode
 */
typedef struct typed_dset {
    /** Number of function pointers which follow.
     * The value depends on the recordtype, but must be >=4 */
    long number;
    /** Called from dbior() */
    long (*report)(int lvl);
    /** Called twice during iocInit().
     * First with @a after = 0 before init_record() or array field allocation.
     * Again with @a after = 1 after init_record() has finished.
     */
    long (*init)(int after);
    /** Called once per record instance */
    long (*init_record)(struct dbCommon *prec);
    /** Called when SCAN="I/O Intr" on startup, or after SCAN is changed.
     *
     * Caller must assign the third arguement (IOCSCANPVT*).  eg.
     @code
     struct mpvt {
        IOSCANPVT drvlist;
     };
     ...
        // init_record() routine calls
        scanIoInit(&pvt->drvlist);
     ...
     static long get_ioint_info(int detach, struct dbCommon *prec, IOCSCANPVT* pscan) {
        if(prec->dpvt)
            *pscan = &((mypvt*)prec->dpvt)->drvlist;
     @endcode
     *
     * When a particular record instance can/will only used a single scan list,
     * the @a detach argument can be ignored.
     *
     * If this is not the case, then the following should be noted.
     * + get_ioint_info() is called with @a detach = 0 to fetch the scan list to
     *   which this record will be added.
     * + get_ioint_info() is called later with @a detach = 1 to fetch the scan
     *   list from which this record should be removed.
     * + Calls will be balanced, so a call with @a detach = 0 will be followed
     *   by one with @a detach = 1.
     *
     * @note get_ioint_info() will be called during IOC shutdown if the
     * dsxt::del_record() extended callback is defined.  (from 3.15.0.1)
     */
    long (*get_ioint_info)(int detach, struct dbCommon *prec, IOSCANPVT* pscan);
    /* Any further functions are specified by the record type. */
} typed_dset;

/** Device support extension table.
 *
 * Optional routines to allow run-time address modifications to be communicated
 * to device support, which must register a struct dsxt by calling devExtend()
 * from its init() routine.
 */
typedef struct dsxt {
    /** Optional, called to offer device support a new record to control.
     *
     * Routine may return a non-zero error code to refuse record.
     */
    long (*add_record)(struct dbCommon *precord);
    /** Optional, called to remove record from device support control.
     *
     * Routine return a non-zero error code to refuse record removal.
     */
    long (*del_record)(struct dbCommon *precord);
    /* Only future Base releases may extend this table. */
} dsxt;

#ifdef __cplusplus
extern "C" {
    typedef long (*DEVSUPFUN)(void *);	/* ptr to device support function*/
#else
    typedef long (*DEVSUPFUN)();	/* ptr to device support function*/
#endif

#ifndef USE_TYPED_DSET

typedef struct dset {   /* device support entry table */
    long	number;		/*number of support routines*/
    DEVSUPFUN	report;		/*print report*/
    DEVSUPFUN	init;		/*init support layer*/
    DEVSUPFUN	init_record;	/*init device for particular record*/
    DEVSUPFUN	get_ioint_info;	/* get io interrupt information*/
    /*other functions are record dependent*/
} dset;

#else
typedef typed_dset dset;
#endif /* USE_TYPED_DSET */

/* exists only to disambiguate  dset dbCommon::dset */
typedef dset unambiguous_dset;

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
