/*************************************************************************\
* Copyright (c) 2012 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* SPDX-License-Identifier: EPICS
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/* recGbl.h */
/*      Record Global
 *      Author:          Marty Kraimer
 *      Date:            13Jun95
 */
#ifndef INCrecGblh
#define INCrecGblh 1

#include <stdarg.h>

#include "compilerDependencies.h"
#include "epicsTypes.h"
#include "dbCoreAPI.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Feature test macro for alarm message (AMSG) field and support
 *
 * Covers addition of dbCommon::amsg, recGblSetSevrMsg(), lset::getAlarmMsg()
 *
 * @since 7.0.6
 */
#define HAS_ALARM_MESSAGE 1

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
DBCORE_API extern RECGBL_ALARM_HOOK_ROUTINE recGblAlarmHook;

/* Global Record Support Routines */

DBCORE_API void recGblDbaddrError(long status, const struct dbAddr *paddr,
    const char *pcaller_name);
DBCORE_API void recGblRecordError(long status, void *precord,
    const char *pcaller_name);
DBCORE_API void recGblRecSupError(long status, const struct dbAddr *paddr,
    const char *pcaller_name, const char *psupport_name);
DBCORE_API void recGblGetGraphicDouble(const struct dbAddr *paddr,
    struct dbr_grDouble *pgd);
DBCORE_API void recGblGetControlDouble(
    const struct dbAddr *paddr, struct dbr_ctrlDouble *pcd);
DBCORE_API void recGblGetAlarmDouble(const struct dbAddr *paddr,
    struct dbr_alDouble *pad);
DBCORE_API void recGblGetPrec(const struct dbAddr *paddr,
    long *pprecision);
DBCORE_API int  recGblInitConstantLink(struct link *plink,
    short dbftype, void *pdest);
DBCORE_API unsigned short recGblResetAlarms(void *precord);
DBCORE_API int recGblSetSevr(void *precord, epicsEnum16 new_stat,
    epicsEnum16 new_sevr);
DBCORE_API void recGblInheritSevr(int msMode, void *precord, epicsEnum16 stat,
    epicsEnum16 sevr);
DBCORE_API int recGblSetSevrMsg(void *precord, epicsEnum16 new_stat,
                                epicsEnum16 new_sevr,
                                EPICS_PRINTF_FMT(const char *msg), ...) EPICS_PRINTF_STYLE(4,5);
DBCORE_API int recGblSetSevrVMsg(void *precord, epicsEnum16 new_stat,
                                 epicsEnum16 new_sevr,
                                 const char *msg, va_list args);
DBCORE_API void recGblFwdLink(void *precord);
DBCORE_API void recGblGetTimeStamp(void *precord);
DBCORE_API void recGblGetTimeStampSimm(void *prec, const epicsEnum16 simm, struct link *siol);
DBCORE_API void recGblCheckDeadband(epicsFloat64 *poldval, const epicsFloat64 newval,
    const epicsFloat64 deadband, unsigned *monitor_mask, const unsigned add_mask);
DBCORE_API void recGblSaveSimm(const epicsEnum16 sscn,
    epicsEnum16 *poldsimm, const epicsEnum16 simm);
DBCORE_API void recGblCheckSimm(struct dbCommon *prec, epicsEnum16 *psscn,
    const epicsEnum16 oldsimm, const epicsEnum16 simm);
DBCORE_API void recGblInitSimm(struct dbCommon *prec, epicsEnum16 *psscn,
    epicsEnum16 *poldsimm, epicsEnum16 *psimm, struct link *psiml);
DBCORE_API long recGblGetSimm(struct dbCommon *prec, epicsEnum16 *psscn,
    epicsEnum16 *poldsimm, epicsEnum16 *psimm, struct link *psiml);

#ifdef __cplusplus
}
#endif

#endif /*INCrecGblh*/
