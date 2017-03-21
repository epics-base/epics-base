/*
 * DO NOT INCLUDE THIS FILE
 *
 * Instead #define UNTYPED_RSET, then #include "recSup.h"
 */

typedef long (*RECSUPFUN) ();      /* ptr to record support function*/

typedef struct rset {	/* record support entry table */
	long		number;		/*number of support routines	*/
	RECSUPFUN	report;		/*print report			*/
	RECSUPFUN	init;		/*init support			*/
	RECSUPFUN	init_record;	/*init record			*/
	RECSUPFUN	process;	/*process record		*/
	RECSUPFUN	special;	/*special processing		*/
	RECSUPFUN	get_value;	/*no longer used		*/
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
