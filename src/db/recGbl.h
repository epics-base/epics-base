/* recGbl.h */
/*	Record Global
 *      Author:          Marty Kraimer
 *      Date:            13Jun95
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
 * Modification Log:
 * -----------------
 * .01  12Jun95		mrk	Removed from recSup.h
 */
#ifndef INCrecGblh
#define INCrecGblh 1


#include "shareLib.h"

/*************************************************************************/

#define recGblSetSevr(PREC,NSTA,NSEV) \
(\
     ((PREC)->nsev<(NSEV))\
     ? ((PREC)->nsta=(NSTA),(PREC)->nsev=(NSEV),TRUE)\
     : FALSE\
)


/* Global Record Support Routines*/
epicsShareFunc void epicsShareAPI recGblDbaddrError(
    long status, struct dbAddr *paddr, char *pcaller_name);
epicsShareFunc void epicsShareAPI recGblRecordError(
    long status, void *precord, char *pcaller_name);
epicsShareFunc void epicsShareAPI recGblRecSupError(
    long status, struct dbAddr *paddr, char *pcaller_name, char *psupport_name);
epicsShareFunc void epicsShareAPI recGblGetGraphicDouble(
    struct dbAddr *paddr, struct dbr_grDouble *pgd);
epicsShareFunc void epicsShareAPI recGblGetControlDouble(
    struct dbAddr *paddr, struct dbr_ctrlDouble *pcd);
epicsShareFunc void epicsShareAPI recGblGetAlarmDouble(
    struct dbAddr *paddr, struct dbr_alDouble *pad);
epicsShareFunc void epicsShareAPI recGblGetPrec(
    struct dbAddr *paddr, long *pprecision);
epicsShareFunc int  epicsShareAPI recGblInitConstantLink(
    struct link *plink,short dbftype,void *pdest);
epicsShareFunc unsigned short epicsShareAPI recGblResetAlarms(void *precord);
epicsShareFunc void epicsShareAPI recGblFwdLink(void *precord);
epicsShareFunc void epicsShareAPI recGblGetTimeStamp(void *precord);
#endif /*INCrecGblh*/
