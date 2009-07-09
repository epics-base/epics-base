/*************************************************************************\
* Copyright (c) 2009 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* recSup.h
 *	Record Support
 *      Author:          Marty Kraimer
 *      Date:            6-1-90
 */

#ifndef INCrecSuph
#define INCrecSuph 1

#include "errMdef.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef long (*RECSUPFUN) ();      /* ptr to record support function*/

typedef struct rset {	/* record support entry table */
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
}rset;

#define RSETNUMBER ( (sizeof(struct rset) - sizeof(long))/sizeof(RECSUPFUN) )


#define S_rec_noRSET     (M_recSup| 1) /*Missing record support entry table*/
#define S_rec_noSizeOffset (M_recSup| 2) /*Missing SizeOffset Routine*/
#define S_rec_outMem     (M_recSup| 3) /*Out of Memory*/


/* Definition os structure for routine get_value */

typedef struct valueDes {
	long	field_type;
	long	no_elements;
	void *	pvalue;
}valueDes;

/************************************************************************
 * report(FILE fp,void *precord);
 * init();
 * init_record(precord,pass);
 * process(void *precord);
 * special(struct dbAddr *paddr, after);
 * get_value(precord,struct valueDes *p);
 * cvt_dbaddr(struct dbAddr *paddr);
 * get_array_info(paddr,long *no_elements,long *offset);
 * put_array_info(paddr,nNew);
 * get_units(paddr,char units[8]);
 * get_precision(struct dbAddr *paddr,long *precision);
 * get_enum_str(paddr,pbuffer);
 * get_enum_strs(paddr,struct dbr_enumStrs *p);
 * put_enum_str(paddr,pbuffer);
 * get_graphic_double(paddr,struct dbr_grDouble *p);
 * get_control_double(paddr,struct dbr_ctrlDouble *p);
 * get_alarm_double(paddr,struct dbr_ctrlDouble *p);
 ***********************************************************************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*INCrecSuph*/
