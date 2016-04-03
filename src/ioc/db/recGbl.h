/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* recGbl.h */
/*	Record Global
 *      Author:          Marty Kraimer
 *      Date:            13Jun95
 */
#ifndef INCrecGblh
#define INCrecGblh 1

#include "epicsTypes.h"
#include "shareLib.h"

#ifdef __cplusplus
extern "C" {
#endif

/*************************************************************************/

/* Structures needed for args */

struct link;
struct dbAddr;
struct dbr_alDouble;
struct dbr_ctrlDouble;
struct dbr_grDouble;
struct dbCommon;

/* Hook Routine */

typedef void (*RECGBL_ALARM_HOOK_ROUTINE)(struct dbCommon *prec,
    epicsEnum16 prev_sevr, epicsEnum16 prev_stat);
epicsShareExtern RECGBL_ALARM_HOOK_ROUTINE recGblAlarmHook;

/* Global Record Support Routines */

epicsShareFunc void recGblDbaddrError(long status, const struct dbAddr *paddr,
    const char *pcaller_name);
epicsShareFunc void recGblRecordError(long status, void *precord,
    const char *pcaller_name);
epicsShareFunc void recGblRecSupError(long status, const struct dbAddr *paddr,
    const char *pcaller_name, const char *psupport_name);
epicsShareFunc void recGblGetGraphicDouble(const struct dbAddr *paddr,
    struct dbr_grDouble *pgd);
epicsShareFunc void recGblGetControlDouble(
    const struct dbAddr *paddr, struct dbr_ctrlDouble *pcd);
epicsShareFunc void recGblGetAlarmDouble(const struct dbAddr *paddr,
    struct dbr_alDouble *pad);
epicsShareFunc void recGblGetPrec(const struct dbAddr *paddr,
    long *pprecision);
epicsShareFunc int  recGblInitConstantLink(struct link *plink,
    short dbftype, void *pdest);
epicsShareFunc unsigned short recGblResetAlarms(void *precord);
epicsShareFunc int recGblSetSevr(void *precord, epicsEnum16 new_stat,
    epicsEnum16 new_sevr);
epicsShareFunc void recGblInheritSevr(int msMode, void *precord, epicsEnum16 stat,
    epicsEnum16 sevr);
epicsShareFunc void recGblFwdLink(void *precord);
epicsShareFunc void recGblGetTimeStamp(void *precord);
epicsShareFunc void recGblTSELwasModified(struct link *plink);
epicsShareFunc void recGblCheckDeadband(epicsFloat64 *poldval, const epicsFloat64 newval,
    const epicsFloat64 deadband, unsigned *monitor_mask, const unsigned add_mask);

#ifdef __cplusplus
}
#endif

#endif /*INCrecGblh*/
