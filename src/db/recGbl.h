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


#ifdef vxWorks
#ifndef INCdbCommonh
#include <dbCommon.h>
#endif
#endif

/*
 *  One ABSOLUTELY must include dbAccess.h before the
 *     definitions in this file
 */

#ifndef INCdbAccessh
#include <dbAccess.h>
#endif
#ifndef INCalarmh
#include <alarm.h>
#endif
#include <dbFldTypes.h>

/*************************************************************************
 * The following must match definitions in global menu definitions
 *************************************************************************/

/* GBL_IVOA */
#define IVOA_CONTINUE         0
#define IVOA_NO_OUTPUT        1
#define IVOA_OUTPUT_IVOV      2

/*************************************************************************/

#define recGblSetSevr(PREC,NSTA,NSEV) \
(\
     ((PREC)->nsev<(NSEV))\
     ? ((PREC)->nsta=(NSTA),(PREC)->nsev=(NSEV),TRUE)\
     : FALSE\
)


/* Global Record Support Routines*/
void recGblDbaddrError(long status, struct dbAddr *paddr, char *pcaller_name);
void recGblRecordError(long status, void *precord, char *pcaller_name);
void recGblRecSupError(long status, struct dbAddr *paddr, char *pcaller_name, char *psupport_name);
void recGblGetGraphicDouble(struct dbAddr *paddr, struct dbr_grDouble *pgd);
void recGblGetControlDouble(struct dbAddr *paddr, struct dbr_ctrlDouble *pcd);
void recGblGetAlarmDouble(struct dbAddr *paddr, struct dbr_alDouble *pad);
void recGblGetPrec(struct dbAddr *paddr, long *pprecision);
int  recGblInitConstantLink(struct link *plink,short dbftype,void *pdest);
unsigned short recGblResetAlarms(void *precord);
void recGblFwdLink(void *precord);
void recGblGetTimeStamp(void *precord);
#endif /*INCrecGblh*/
