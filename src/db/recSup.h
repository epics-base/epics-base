/* recSup.h
 *	Record Support
 *      Author:          Marty Kraimer
 *      Date:            6-1-90
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
 * .01  11-11-91        jba     Added include dbCommon.h,recGblSetSevr,recGblResetSevr 
 * .02  12-18-91        jba     Changed caddr_t to void *
 * .03  03-04-92        jba     Added include for dbAccess.h
 * .04  05-18-92        rcz     removed extern
 * .05  05-18-92        rcz     Changed macro "GET_PRSET(" to "GET_PRSET(precSup,"
 * .06  05-18-92        rcz     New database access (removed extern)
 * .07  07-16-92        jba     Added macro recGblFwdLink
 * .08  07-16-92        jba     changed VALID_ALARM to INVALID_ALARM
 * .09  08-10-92        jba     added #defines for SIMM processing
 * .10  08-11-92        jba     added DB_INTEREST masks
 * .11  08-13-92        jba     added prototype for recGblGetAlarmDouble,
 * .12  08-14-92        jba     added prototypes recGblGetLinkValue,recGblPutLinkValue
 * .13  09-15-92        jba     added vxWorks ifdef
 * .14  -7-27-93	mrk	remove recGblResetSevr add recGblResetAlarms
 * .15	03-18-94	mcn	added fast link macros and prototypes
 * .16	06-13-95	mrk	moved recGbl defs to recGbl.h
 */

#ifndef INCrecSuph
#define INCrecSuph 1
#include <recGbl.h>
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
