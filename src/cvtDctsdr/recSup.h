/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* $Id$
 *	Record Support
 *      Author:          Marty Kraimer
 *      Date:            6-1-90
 *
 */
#ifndef INCrecSuph
#define INCrecSuph 1


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

/*************************************************************************
 * The following must match definitions in choiceGbl.ascii
 *************************************************************************/

/* GBL_IVOA */
#define IVOA_CONTINUE         0
#define IVOA_NO_OUTPUT        1
#define IVOA_OUTPUT_IVOV      2

/*************************************************************************/

#define DB_INTEREST_SCAN        0x00000001
#define DB_INTEREST_CONTROL     0x00000002
#define DB_INTEREST_ALARMS      0x00000004
#define DB_INTEREST_DISPLAY     0x00000008
#define DB_INTEREST_IO          0x00000010
#define DB_INTEREST_CONVERT     0x00000020
#define DB_INTEREST_PRIVATE     0x00000040

#define recGblGetFastLink(PLNK,PREC,PVAL) \
\
  ((((PLNK)->type == CONSTANT) ||\
      (! (((PLNK)->type == DB_LINK) || ((PLNK)->type == CA_LINK)) ))\
  ? 0\
  : dbFastLinkGet((PLNK),(struct dbCommon *)(PREC),(PVAL)))
 
 
#define recGblPutFastLink(PLNK,PREC,PVAL) \
\
  ((((PLNK)->type == CONSTANT) ||\
      (! (((PLNK)->type == DB_LINK) || ((PLNK)->type == CA_LINK)) ))\
  ? 0\
  : dbFastLinkPut((PLNK),(struct dbCommon *)(PREC),(PVAL)))


#define recGblSetSevr(PREC,NSTA,NSEV) \
(\
     ((PREC)->nsev<(NSEV))\
     ? ((PREC)->nsta=(NSTA),(PREC)->nsev=(NSEV),TRUE)\
     : FALSE\
)

typedef long (*RECSUPFUN) ();	/* ptr to record support function*/

struct rset {	/* record support entry table */
	long		number;		/*number of support routines	*/
	RECSUPFUN	report;		/*print report			*/
	RECSUPFUN	init;		/*init support			*/
	RECSUPFUN	init_record;	/*init record			*/
	RECSUPFUN	process;	/*process record		*/
	RECSUPFUN	special;	/*special processing		*/
	RECSUPFUN	get_value;	/*get value field		*/
	RECSUPFUN	cvt_dbaddr;	/*cvt  dbAddr			*/
	RECSUPFUN	get_array_info;
	RECSUPFUN	put_array_info;
	RECSUPFUN	get_units;
	RECSUPFUN	get_precision;
	RECSUPFUN	get_enum_str;	/*get string from enum item*/
	RECSUPFUN	get_enum_strs;	/*get all enum strings		*/
	RECSUPFUN	put_enum_str;	/*put string from enum item*/
	RECSUPFUN	get_graphic_double;
	RECSUPFUN	get_control_double;
	RECSUPFUN	get_alarm_double;
	};
struct recSup {
	long		number;		/*number of record types	*/
	struct rset	**papRset;	/*ptr to arr of ptr to rset	*/
	};
#define RSETNUMBER ( (sizeof(struct rset) - sizeof(long))/sizeof(RECSUPFUN) )

#define S_rec_noRSET     (M_recSup| 1) /*Missing record support entry table*/
#define S_rec_outMem     (M_recSup| 3) /*Out of Memory*/

/*************************************************************************
 *
 * The following is a valid reference
 *
 * precSup->papRset[1]->report(<args>)
 *************************************************************************/


/* Definition os structure for routine get_value */

struct valueDes {
	long	field_type;
	long	no_elements;
	void *	pvalue;
};

/**************************************************************************
 * The following macro returns either the addr of rset or NULL
 * A typical usage is:
 *
 *	struct rset *prset;
 *	if(!(prset=GET_PRSET(precSup,type))) {<null}
 ***********************************************************************/
#define GET_PRSET(PRECSUP,REC_TYPE)\
(\
    (PRECSUP)\
    ?(\
	(((REC_TYPE) < 1) || ((REC_TYPE) >= (PRECSUP)->number))\
	?   NULL\
	:   ((PRECSUP)->papRset[(REC_TYPE)])\
    )\
    : NULL\
)



/************************************************************************
 * report(FILE fp,void *precord);
 * init();
 * init_record(precord,pass);
 * process(void *precord);
 * special(struct db_addr *paddr, after);
 * get_value(precord,struct valueDes *p);
 * cvt_dbaddr(struct db_addr *paddr);
 * get_array_info(paddr,long *no_elements,long *offset);
 * put_array_info(paddr,nNew);
 * get_units(paddr,char units[8]);
 * get_precision(struct db_addr *paddr,long *precision);
 * get_enum_str(paddr,pbuffer);
 * get_enum_strs(paddr,struct dbr_enumStrs *p);
 * put_enum_str(paddr,pbuffer);
 * get_graphic_double(paddr,struct dbr_grDouble *p);
 * get_control_double(paddr,struct dbr_ctrlDouble *p);
 * get_alarm_double(paddr,struct dbr_ctrlDouble *p);
 ***********************************************************************/

/* Global Record Support Routines*/
#ifdef __STDC__

void recGblDbaddrError(long status, struct dbAddr *paddr, char *pcaller_name);
void recGblRecordError(long status, void *precord, char *pcaller_name);
void recGblRecSupError(long status, struct dbAddr *paddr, char *pcaller_name, char *psupport_name);
void recGblGetGraphicDouble(struct dbAddr *paddr, struct dbr_grDouble *pgd);
void recGblGetControlDouble(struct dbAddr *paddr, struct dbr_ctrlDouble *pcd);
void recGblGetAlarmDouble(struct dbAddr *paddr, struct dbr_alDouble *pad);
void recGblGetPrec(struct dbAddr *paddr, long *pprecision);
long recGblGetLinkValue(struct link *plink,void *precord,
	short dbrType,void *pdest,long *poptions,long *pnRequest);
long recGblPutLinkValue(struct link *plink,void *precord,
	short dbrType,void *pdest,long *pnRequest);
unsigned short recGblResetAlarms(void *precord);
void recGblFwdLink(void *precord);
void recGblGetTimeStamp(void *precord);
long recGblInitFastInLink(struct link *plink, void *precord, short dbrType, char *fld_name);
long recGblInitFastOutLink(struct link *plink, void *precord, short dbrType, char *fld_name);

#else

void recGblDbaddrError();
void recGblRecordError();
void recGblRecSupError();
void recGblGetGraphicDouble();
void recGblGetControlDouble();
void recGblGetAlarmDouble();
void recGblGetPrec();
long recGblGetLinkValue();
long recGblPutLinkValue();
unsigned short recGblResetAlarms();
long recGblInitFastInLink();
long recGblInitFastOutLink();

#endif /*__STDC__*/

#endif /*INCrecSuph*/
