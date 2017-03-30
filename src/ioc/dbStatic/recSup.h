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
#include "compilerDependencies.h"

#ifdef __cplusplus
extern "C" {
#endif

/* RSET definition */

/* defined elsewhere */
struct dbAddr;
struct dbCommon;
struct dbr_enumStrs;
struct dbr_grDouble;
struct dbr_ctrlDouble;
struct dbr_alDouble;

/* record support entry table */
struct typed_rset {
    long number; /* number of support routines */
    long (*report)(void *precord);
    long (*init)();
    long (*init_record)(struct dbCommon *precord, int pass);
    long (*process)(struct dbCommon *precord);
    long (*special)(struct dbAddr *paddr, int after);
    long (*get_value)(void); /* DEPRECATED set to NULL */
    long (*cvt_dbaddr)(struct dbAddr *paddr);
    long (*get_array_info)(struct dbAddr *paddr, long *no_elements, long *offset);
    long (*put_array_info)(struct dbAddr *paddr, long nNew);
    long (*get_units)(struct dbAddr *paddr, char *units);
    long (*get_precision)(const struct dbAddr *paddr, long *precision);
    long (*get_enum_str)(const struct dbAddr *paddr, char *pbuffer);
    long (*get_enum_strs)(const struct dbAddr *paddr, struct dbr_enumStrs *p);
    long (*put_enum_str)(const struct dbAddr *paddr, const char *pbuffer);
    long (*get_graphic_double)(struct dbAddr *paddr, struct dbr_grDouble *p);
    long (*get_control_double)(struct dbAddr *paddr, struct dbr_ctrlDouble *p);
    long (*get_alarm_double)(struct dbAddr *paddr, struct dbr_alDouble *p);
};

#ifdef USE_TYPED_RSET

typedef struct typed_rset rset;

#else

/* pre-3.16 old untyped RSET definition - DEPRECATED */

typedef long (*RECSUPFUN) () EPICS_DEPRECATED;      /* ptr to record support function*/

struct rset {	/* record support entry table */
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
} EPICS_DEPRECATED;

typedef struct rset rset EPICS_DEPRECATED;

#endif

#define RSETNUMBER 17

#define S_rec_noRSET     (M_recSup| 1) /*Missing record support entry table*/
#define S_rec_noSizeOffset (M_recSup| 2) /*Missing SizeOffset Routine*/
#define S_rec_outMem     (M_recSup| 3) /*Out of Memory*/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /*INCrecSuph*/
