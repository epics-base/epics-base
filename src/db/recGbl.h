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
 *
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
