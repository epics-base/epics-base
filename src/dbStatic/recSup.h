/*************************************************************************\
* Copyright (c) 2002 The University of Chicago, as Operator of Argonne
*     National Laboratory.
* Copyright (c) 2002 The Regents of the University of California, as
*     Operator of Los Alamos National Laboratory.
* EPICS BASE Versions 3.13.7
* and higher are distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/
/* recSup.h
 *	Record Support
 *      Author:          Marty Kraimer
 *      Date:            6-1-90
 */

#ifndef INCrecSuph
#define INCrecSuph 1

typedef long (*RECSUPFUN) ();      /* ptr to record support function*/

#ifdef __cplusplus
#include <dbAddr.h>
extern "C" 
{
    struct rset {	// record support entry table
        long number;                                // no of support routines
        long (*report) ( const dbCommon * precord );          // print report
        long (*init) ();                                      // init support
        long (*init_record) ( dbCommon * precord, int pass );  // init record
        long (*process) ( dbCommon * precord );             // process record
        long (*special) ( DBADDR * paddr, int after );  // special processing
        long (*get_value) ();                                     // obsolete
        long (*cvt_dbaddr) ( DBADDR * paddr );              // convert dbaddr
        long (*get_array_info) ( const DBADDR * paddr,
                                 long * no_elements, long * offset );
        long (*put_array_info) ( const DBADDR * paddr, const long nNew );
        long (*get_units) ( const DBADDR * paddr, char * units );
        long (*get_precision) ( const DBADDR * paddr, long * precision );
        long (*get_enum_str) ( const DBADDR * paddr, char * pstring );
        long (*get_enum_strs) ( const DBADDR * paddr,
                                struct dbr_enumStrs * pes );
        long (*put_enum_str) ( const DBADDR * paddr, const char * pstring );
        long (*get_graphic_double) ( const DBADDR * paddr,
                                     struct dbr_grDouble * pgd );
        long (*get_control_double) ( const DBADDR * paddr,
                                     struct dbr_ctrlDouble * pcd );
        long (*get_alarm_double) ( const DBADDR * paddr,
                                   struct dbr_alDouble * pad );
    };
}
#else
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
#endif /* ifdef __cplusplus else */

#define RSETNUMBER ( (sizeof(struct rset) - sizeof(long))/sizeof(RECSUPFUN) )


#define S_rec_noRSET     (M_recSup| 1) /*Missing record support entry table*/
#define S_rec_noSizeOffset (M_recSup| 2) /*Missing SizeOffset Routine*/
#define S_rec_outMem     (M_recSup| 3) /*Out of Memory*/


/* Definition os structure for routine get_value */

struct valueDes {
	long	field_type;
	long	no_elements;
	void *	pvalue;
};

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
#endif /*INCrecSuph*/
