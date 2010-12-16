/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* recGbl.h */
/*	Record Global
 *      Author:          Marty Kraimer
 *      Date:            13Jun95
 */
#ifndef INCrecGblh
#define INCrecGblh 1

#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/*************************************************************************/

#define recGblSetSevr(PREC,NSTA,NSEV) \
(\
     ((PREC)->nsev<(NSEV))\
     ? ((PREC)->nsta=(NSTA),(PREC)->nsev=(NSEV),TRUE)\
     : FALSE\
)

/* Structures needed for args */

struct link;
struct dbAddr;
struct dbr_alDouble;
struct dbr_ctrlDouble;
struct dbr_grDouble;
struct dbCommon;

/* Hook Routine */

typedef void (*RECGBL_ALARM_HOOK_ROUTINE)(struct dbCommon *prec,
    unsigned short prev_sevr, unsigned short prev_stat);
extern RECGBL_ALARM_HOOK_ROUTINE recGblAlarmHook;

/* Global Record Support Routines */

epicsShareFunc void epicsShareAPI recGblDbaddrError(
    long status, const struct dbAddr *paddr, const char *pcaller_name);
epicsShareFunc void epicsShareAPI recGblRecordError(
    long status, void *precord, const char *pcaller_name);
epicsShareFunc void epicsShareAPI recGblRecSupError(
    long status, const struct dbAddr *paddr, const char *pcaller_name, const char *psupport_name);
epicsShareFunc void epicsShareAPI recGblGetGraphicDouble(
    const struct dbAddr *paddr, struct dbr_grDouble *pgd);
epicsShareFunc void epicsShareAPI recGblGetControlDouble(
    const struct dbAddr *paddr, struct dbr_ctrlDouble *pcd);
epicsShareFunc void epicsShareAPI recGblGetAlarmDouble(
    const struct dbAddr *paddr, struct dbr_alDouble *pad);
epicsShareFunc void epicsShareAPI recGblGetPrec(
    const struct dbAddr *paddr, long *pprecision);
epicsShareFunc int  epicsShareAPI recGblInitConstantLink(
    struct link *plink,short dbftype,void *pdest);
epicsShareFunc unsigned short epicsShareAPI recGblResetAlarms(void *precord);
epicsShareFunc void epicsShareAPI recGblFwdLink(void *precord);
epicsShareFunc void epicsShareAPI recGblGetTimeStamp(void *precord);
epicsShareFunc void epicsShareAPI recGblTSELwasModified(struct link *plink);

#ifdef __cplusplus
}
#endif

#endif /*INCrecGblh*/
